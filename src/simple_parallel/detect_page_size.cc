#include <fstream>
#include <iostream>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // make sure we have enough arguments
  if (argc < 2) {
    return 1;
  }

  std::ofstream fout(argv[1], std::ios_base::out);
  const bool fileOpen = fout.is_open();
  if (fileOpen) {
    fout << "#pragma once\n\n"
         << "#include <cstddef>\n\n"
         << "namespace simple_parallel {\n"
         << "constexpr size_t page_size = " << sysconf(_SC_PAGE_SIZE) << ";\n"
         << "} // namespace simple_parallel\n";
    fout.close();
  }
  return fileOpen ? 0 : 1; // return 0 if wrote the file
}
