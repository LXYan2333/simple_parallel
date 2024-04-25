// copied from
// https://gcc.gnu.org/onlinedocs/gcc/Nested-Functions.html#Nested-Functions-1

int main() {
    int square(int z) { return z * z; }

    return square(1) + square(2);
}
