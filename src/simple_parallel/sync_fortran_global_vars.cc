#include <algorithm>
#include <boost/mpi.hpp>
#include <cstddef>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <gelf.h>
#include <gsl/gsl>
#include <gsl/util>
#include <libelf.h>
#include <link.h>
#include <optional>
#include <ranges>
#include <re2/re2.h>
#include <simple_parallel/cxx/simple_parallel.h>
#include <simple_parallel/cxx/types_fwd.h>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <sync_fortran_global_vars.h>

using namespace std::literals;

namespace simple_parallel {
auto symbol_name_2_addr(std::string_view name) -> std::optional<char *> {
#ifndef _GNU_SOURCE
#error The _GNU_SOURCE feature test macro must be defined in order to obtain the definitions of RTLD_DEFAULT and RTLD_NEXT from <dlfcn.h>.
#endif
  //  NOLINTNEXTLINE(*pointer-arithmetic,*suspicious-stringview-data-usage)
  char *addr = static_cast<char *>(dlsym(RTLD_DEFAULT, name.data()));
  if (addr == nullptr) {
    // NOLINTNEXTLINE(*-mt-unsafe)
    char *dlerror_ret = dlerror();
    if (dlerror_ret != nullptr) {
      return std::nullopt;
    }
  }
  return addr;
}

namespace {

struct global_variable_info {
  std::string_view name;
  mem_area area;
};

// copied from https://stackoverflow.com/a/57099317/18245120
// slightly modified to obey strict aliasing rule
auto get_sym_num_from_gnu_hash(Elf64_Addr gnuHashAddress) -> uint32_t {
  // NOLINTBEGIN

  // See https://flapenguin.me/2017/05/10/elf-lookup-dt-gnu-hash/ and
  // https://sourceware.org/ml/binutils/2006-10/msg00377.html
  typedef struct {
    uint32_t nbuckets;
    uint32_t symoffset;
    uint32_t bloom_size;
    uint32_t bloom_shift;
  } Header;

  Header header{};
  std::memcpy(&header, reinterpret_cast<void *>(gnuHashAddress),
              sizeof(header));
  const void *bucketsAddress = reinterpret_cast<uint8_t *>(gnuHashAddress) +
                               sizeof(Header) +
                               (sizeof(uint64_t) * header.bloom_size);

  // Locate the chain that handles the largest index bucket.
  uint32_t lastSymbol = 0;
  const uint32_t *bucketAddress =
      reinterpret_cast<const uint32_t *>(bucketsAddress);
  for (uint32_t i = 0; i < header.nbuckets; ++i) {
    uint32_t bucket{};
    std::memcpy(&bucket, bucketAddress, sizeof(bucket));
    if (lastSymbol < bucket) {
      lastSymbol = bucket;
    }
    bucketAddress++;
  }

  if (lastSymbol < header.symoffset) {
    return header.symoffset;
  }

  // Walk the bucket's chain to add the chain length to the total.
  const void *chainBaseAddress =
      reinterpret_cast<const uint8_t *>(bucketsAddress) +
      (sizeof(uint32_t) * header.nbuckets);
  for (;;) {
    const uint32_t *chainEntry = reinterpret_cast<const uint32_t *>(
        reinterpret_cast<const uint8_t *>(chainBaseAddress) +
        (lastSymbol - header.symoffset) * sizeof(uint32_t));
    lastSymbol++;

    // If the low bit is set, this entry is the end of the chain.
    uint32_t end_bit{};
    std::memcpy(&end_bit, chainEntry, sizeof(end_bit));
    if (end_bit & 1) {
      break;
    }
  }

  return lastSymbol;
  // NOLINTEND
}

auto d_tag(const ElfW(Dyn) & e) -> ElfW(Sxword) { return e.d_tag; };

auto get_sym_cnt(std::span<const ElfW(Dyn)> dynamic_tags) -> ElfW(Word) {

  auto hash_tag = std::ranges::find(dynamic_tags, DT_HASH, d_tag);
  if (hash_tag != dynamic_tags.end()) {
    // NOLINTNEXTLINE(*reinterpret-cast,*no-int-to-ptr,*union-access)
    auto *hash = reinterpret_cast<ElfW(Word *)>(hash_tag->d_un.d_ptr);
    // The 2nd word is the number of symbols
    // NOLINTNEXTLINE(*pointer-arithmetic)
    const ElfW(Word) sym_cnt = hash[1];
    BOOST_ASSERT(sym_cnt > 0);
    return sym_cnt;
  }

  auto gnu_hash_tag = std::ranges::find(dynamic_tags, DT_GNU_HASH, d_tag);
  if (gnu_hash_tag != dynamic_tags.end()) {
    // NOLINTNEXTLINE(*union-access)
    ElfW(Word) sym_cnt = get_sym_num_from_gnu_hash(gnu_hash_tag->d_un.d_ptr);
    BOOST_ASSERT(sym_cnt > 0);
    return sym_cnt;
  };

  throw std::runtime_error(
      "DT_HASH and DT_GNU_HASH does not present in dynamic_tags");
}

auto get_strtab(std::span<const ElfW(Dyn)> dynamic_tags) -> char * {
  auto strtab = std::ranges::find(dynamic_tags, DT_STRTAB, d_tag);
  if (strtab != dynamic_tags.end()) {
    // NOLINTNEXTLINE(*reinterpret-cast,*no-int-to-ptr,*union-access)
    return reinterpret_cast<char *>(strtab->d_un.d_ptr);
  }

  throw std::runtime_error("DT_STRTAB does not present in dynamic_tags");
}

auto is_fortran_global_variable(std::string_view symbol_name) -> bool {

  // e.g. a `double precision,allocatable,target::TtTt(:,:)` in module
  // `TeSt_ModulE` will have symbol:
  //
  // gfortran:  __test_module_MOD_tttt
  // ifx:       test_module_mp_tttt_
  // flang:     _QMtest_moduleEtttt
  //
  // and those symbols needs to be filter out:
  // libgfortran: __ieee_exceptions_MOD_ieee_usual
  // __ieee_exceptions_MOD_ieee_all
  //
  // see https://godbolt.org/z/q5WTevje8

  static RE2 gfort_global_var_sym_reg("__[^_A-Z][^A-Z]*_MOD_[^_A-Z][^A-Z]*");
  static RE2 ifx_global_var_sym_reg("[^_A-Z][^A-Z]*_mp_[^_A-Z][^A-Z]*_");
  static RE2 flang_global_var_sym_reg("_QM[^_A-Z][^A-Z]*E[^_A-Z][^A-Z]*");

  assert(gfort_global_var_sym_reg.ok());
  assert(ifx_global_var_sym_reg.ok());
  assert(flang_global_var_sym_reg.ok());

  if ((symbol_name == "__ieee_exceptions_MOD_ieee_usual") or
      (symbol_name == "__ieee_exceptions_MOD_ieee_all")) {
    return false;
  }

  if (RE2::FullMatch(symbol_name, gfort_global_var_sym_reg)) {
    return true;
  }

  if (RE2::FullMatch(symbol_name, ifx_global_var_sym_reg)) {
    return true;
  }

  if (RE2::FullMatch(symbol_name, flang_global_var_sym_reg)) {
    return true;
  }

  return false;
}

auto get_desired_section_num() -> std::vector<ElfW(Section)> {

  if (elf_version(EV_CURRENT) == EV_NONE) {
    std::stringstream ss;
    ss << "ELF library initialization failed: " << elf_errmsg(-1) << '\n';
    throw std::runtime_error(std::move(ss).str());
  }

  // NOLINTNEXTLINE(*-vararg)
  int my_exe_fd = open("/proc/self/exe", O_RDONLY);
  if (my_exe_fd == -1) {
    std::stringstream ss;
    ss << "Failed to open /proc/self/exe, reason: "
       // NOLINTNEXTLINE(concurrency-mt-unsafe)
       << std::strerror(errno);
    throw std::runtime_error(std::move(ss).str());
  }

  auto close_my_exe_fd = gsl::finally([&]() {
    if (close(my_exe_fd) == -1) {
      perror("close");
    };
  });

  Elf *elf = elf_begin(my_exe_fd, ELF_C_READ, nullptr);
  if (elf == nullptr) {
    std::stringstream ss;
    ss << "elf_begin() failed: " << elf_errmsg(-1) << '\n';
    throw std::runtime_error(std::move(ss).str());
  }
  auto close_elf = gsl::finally([&]() { elf_end(elf); });

  if (elf_kind(elf) != ELF_K_ELF) {
    throw std::runtime_error("/proc/self/exe is not an ELF object.");
  }

  size_t shstrndx{};
  if (elf_getshdrstrndx(elf, &shstrndx) != 0) {
    std::stringstream ss;
    ss << "elf_getshdrstrndx() failed: " << elf_errmsg(-1) << '\n';
    throw std::runtime_error(std::move(ss).str());
  }

  Elf_Scn *scn = nullptr;
  std::vector<ElfW(Section)> ret;
  while ((scn = elf_nextscn(elf, scn)) != nullptr) {
    GElf_Shdr shdr;
    if (gelf_getshdr(scn, &shdr) != &shdr) {
      std::stringstream ss;
      ss << "getshdr() failed: " << elf_errmsg(-1) << '\n';
      throw std::runtime_error(std::move(ss).str());
    }

    char *name = elf_strptr(elf, shstrndx, shdr.sh_name);
    if (name == nullptr) {
      std::stringstream ss;
      ss << "elf_strptr() failed: " << elf_errmsg(-1) << '\n';
      throw std::runtime_error(std::move(ss).str());
    }

    if (name == ".data"sv or name == ".bss"sv or name == ".dynamic"sv) {
      ret.push_back(gsl::narrow_cast<ElfW(Section)>(elf_ndxscn(scn)));
    }
  }

  return ret;
}

void get_symbol_from_symtab(
    std::span<const ElfW(Sym)> symtab, const char *strtab,
    std::vector<global_variable_info> &global_varaibles_vec) {

  auto is_global_variable = [](const ElfW(Sym) & sym) {
    return ELF64_ST_TYPE(sym.st_info) == STT_OBJECT;
  };
  auto global_variable_syms = symtab | std::views::filter(is_global_variable);

  // we only cares about global variables in .data, .bss and .dynamic section
  static const std::vector<ElfW(Section)> shndx_to_include =
      get_desired_section_num();

  for (const auto &sym : global_variable_syms) {

    // symbol in data, bss and dynamic section?
    if (std::ranges::find(shndx_to_include, sym.st_shndx) ==
        shndx_to_include.end()) {
      continue;
    }

    // If the value is non-zero, it represents a string table index that gives
    // the symbol name.
    // Otherwise, the symbol table entry has no name.
    const ptrdiff_t name_offset = sym.st_name;
    BOOST_ASSERT(name_offset != 0);

    // NOLINTNEXTLINE(*pointer-arithmetic)
    std::string_view sym_name = strtab + name_offset;

    if (is_fortran_global_variable(sym_name)) {
      std::optional<char *> addr = symbol_name_2_addr(sym_name);
      if (!addr.has_value()) {
        // NOLINTNEXTLINE(*-mt-unsafe)
        throw std::runtime_error(dlerror());
      }
      global_varaibles_vec.push_back({sym_name, {*addr, sym.st_size}});

      if (debug()) {
        std::cout << "Adding Fortran global variable " << sym_name
                  << " at address: " << static_cast<void *>(*addr)
                  << ", size: " << sym.st_size << " to syncing list\n";
      }
    }
  }
}

void get_symbol_from_dynamic_tags(
    std::span<const ElfW(Dyn)> dynamic_tags,
    std::vector<global_variable_info> &global_varaibles_vec) {

  // symbol count in this dynamic segment
  ElfW(Word) sym_cnt = get_sym_cnt(dynamic_tags);

  // symbol table
  auto symtab = std::ranges::find(dynamic_tags, DT_SYMTAB, d_tag);
  if (symtab == dynamic_tags.end()) {
    throw std::runtime_error("DT_SYMTAB does not present in dynamic_tags");
  }

  // symbol string table is seperate
  char *strtab = get_strtab(dynamic_tags);
  // NOLINTNEXTLINE(*reinterpret-cast,*no-int-to-ptr,*union-access)
  auto *symtab_ptr = reinterpret_cast<ElfW(Sym *)>(symtab->d_un.d_ptr);

  get_symbol_from_symtab({symtab_ptr, sym_cnt}, strtab, global_varaibles_vec);
}

struct dl_iterate_phdr_param {
  std::vector<global_variable_info> global_varaibles_location;
};

auto retrieve_symbolnames(struct dl_phdr_info *info, size_t /*size*/,
                          void *data) -> int {
  auto *param = static_cast<dl_iterate_phdr_param *>(data);

  const std::string_view object_name = info->dlpi_name;

  if (object_name == "linux-vdso.so.1") {
    // jump to next program header because linux-vdso.so.1's hash address
    // cause segment fault
    return 0;
  }

  if (object_name.find("/libgfortran.so") != object_name.npos) {
    return 0;
  }

  std::span<const ElfW(Phdr)> program_headers{info->dlpi_phdr,
                                              info->dlpi_phnum};
  auto is_dynamic = [](const ElfW(Phdr) & e) { return e.p_type == PT_DYNAMIC; };
  auto dynamic_headers = program_headers | std::views::filter(is_dynamic);

  for (const auto &dynamic_header : dynamic_headers) {

    // NOLINTNEXTLINE(*reinterpret-cast,*no-int-to-ptr)
    const auto *dyn_begin = reinterpret_cast<const ElfW(Dyn) *>(
        info->dlpi_addr + dynamic_header.p_vaddr);

    const auto *dyn_end = dyn_begin;
    while (dyn_end->d_tag != DT_NULL) {
      // NOLINTNEXTLINE(*pointer-arithmetic)
      dyn_end++;
    }

    // NOLINTNEXTLINE(*reinterpret-cast,*no-int-to-ptr)
    get_symbol_from_dynamic_tags({dyn_begin, dyn_end},
                                 param->global_varaibles_location);
  }

  // return 0 for next program header
  return 0;
}

auto get_all_fortran_global_varaibles() -> std::vector<global_variable_info> {

  dl_iterate_phdr_param data{};

  dl_iterate_phdr(retrieve_symbolnames, &data);

  return std::move(data.global_varaibles_location);
}

#ifndef BOOST_ASSERT_IS_VOID
void verify_fortran_global_variables_synced(
    const bmpi::communicator &comm,
    const std::vector<global_variable_info> &fortran_global_variables) {

  const size_t var_count = fortran_global_variables.size();
  std::vector<size_t> all_process_var_count;
  bmpi::all_gather(comm, var_count, all_process_var_count);
  BOOST_ASSERT(std::ranges::all_of(all_process_var_count,
                                   [&](size_t e) { return e == var_count; }));

  for (const auto &i : fortran_global_variables) {
    std::vector<global_variable_info> all_process_info;
    all_process_info.resize(gsl::narrow_cast<size_t>(comm.size()));
    MPI_Allgather(&i, sizeof(global_variable_info), MPI_BYTE,
                  all_process_info.data(), sizeof(global_variable_info),
                  MPI_BYTE, comm);

    for (const auto &j : all_process_info) {
      BOOST_ASSERT(i.area.data() == j.area.data());
      BOOST_ASSERT(i.area.size_bytes() == j.area.size_bytes());
      BOOST_ASSERT(i.name.data() == j.name.data());
      BOOST_ASSERT(i.name.size() == j.name.size());
    }
  }
}
#endif

} // namespace

// fortran programmers prefer using global variables in modules, thus must
// be synced
void sync_fortran_global_variables(const bmpi::communicator &comm,
                                   int root_rank) {

  BOOST_ASSERT(comm.size() > 1);

  static const std::vector<global_variable_info> fortran_global_variables =
      get_all_fortran_global_varaibles();

#ifndef BOOST_ASSERT_IS_VOID
  verify_fortran_global_variables_synced(comm, fortran_global_variables);
#endif

  // wait request with a buffer of 256 requests
  std::vector<MPI_Request> buffer{};
  const size_t buffer_size = 256;
  buffer.reserve(buffer_size);

  for (const auto &i : fortran_global_variables) {
    BOOST_ASSERT(i.area.size_bytes() <= std::numeric_limits<int>::max());
    MPI_Request request{};
    MPI_Ibcast(i.area.data(), gsl::narrow_cast<int>(i.area.size_bytes()),
               MPI_BYTE, root_rank, comm, &request);

    if (buffer.size() == buffer_size) {
      int index{};
      MPI_Waitany(gsl::narrow_cast<int>(buffer.size()), buffer.data(), &index,
                  MPI_STATUS_IGNORE);
      BOOST_ASSERT(index != MPI_UNDEFINED);
      buffer[gsl::narrow_cast<size_t>(index)] = request;
    } else {
      buffer.push_back(request);
    }
  }

  MPI_Waitall(gsl::narrow_cast<int>(buffer.size()), buffer.data(),
              MPI_STATUS_IGNORE);
}

} // namespace simple_parallel