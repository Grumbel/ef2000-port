// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Exercise Unicorn's x87 floating-point *memory access* path. Some static
// Unicorn 2.1.4 archives contain GOTPCRELX relocations in QEMU's memory
// helper trampolines which GNU ld must not relax (see batch 104 notes).

#include <unicorn/unicorn.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

void checked(uc_err error, const char* step)
{
    if (error != UC_ERR_OK) {
        std::fprintf(stderr, "unicorn-x87: %s: %s\n", step, uc_strerror(error));
        throw error;
    }
}

} // namespace

int main()
{
    uc_engine* cpu = nullptr;
    try {
        checked(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu), "open");
        checked(uc_mem_map(cpu, 0, 0x3000, UC_PROT_ALL), "map");
        // fld qword ptr [0x1000]; fstp qword ptr [0x1008]; hlt
        constexpr std::array<uint8_t, 13> code{
            0xdd, 0x05, 0x00, 0x10, 0, 0,
            0xdd, 0x1d, 0x08, 0x10, 0, 0,
            0xf4
        };
        checked(uc_mem_write(cpu, 0x200, code.data(), code.size()), "code");
        constexpr std::array<double, 5> values{3.25, -0.5, 123456.125,
                                                0.0, 0.0000152587890625};
        for (double value : values) {
            checked(uc_mem_write(cpu, 0x1000, &value, sizeof(value)), "input");
            checked(uc_emu_start(cpu, 0x200, 0, 0, 3), "execute");
            double result = 0;
            checked(uc_mem_read(cpu, 0x1008, &result, sizeof(result)), "output");
            if (std::memcmp(&result, &value, sizeof(value)) != 0) {
                std::fprintf(stderr, "unicorn-x87: FLD/FSTP changed %.17g to %.17g\n",
                             value, result);
                uc_close(cpu);
                return 1;
            }
        }
        checked(uc_close(cpu), "close");
        std::puts("unicorn-x87: 5/5 FLD/FSTP qword memory round-trips passed");
        return 0;
    } catch (uc_err) {
        if (cpu) uc_close(cpu);
        return 1;
    }
}
