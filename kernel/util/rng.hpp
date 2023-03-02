#pragma once

namespace rng {
    template <int N>
    consteval unsigned long __seed_tmstmp(char const (&s)[N]) {
        unsigned long res = 1337;
        for (int i = 0; i < N; i++) {
            res += s[i];
        }
        for (int i = 0; i < N; i++) {
            res -= s[i] * s[N - i - 1];
        }
        for (int i = 0; i < N - 1; i++) {
            if (s[i + 1] != 0) res += s[i] / s[i + 1];
        }
        return res * 1103515245 + 12345;
    }

    consteval unsigned long compile_time_seed(void) {
        return __seed_tmstmp(__TIME__);
    }

    inline int rand(unsigned long &state) {
        state = state * 1103515245 + 12345;
        return (unsigned int)(state / 65536) % 32768;
    }
}
