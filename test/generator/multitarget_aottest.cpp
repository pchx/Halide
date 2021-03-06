#include <atomic>
#include <string>
#include <tuple>
#include "HalideRuntime.h"
#include "multitarget.h"
#include "HalideBuffer.h"

using namespace Halide::Runtime;

void my_error_handler(void *user_context, const char *message) {
    printf("Saw Error: (%s)\n", message);
}

std::pair<std::string, bool> get_env_variable(char const *env_var_name) {
    if (env_var_name) {
        size_t read = 0;
        #ifdef _MSC_VER
        char lvl[32];
        getenv_s(&read, lvl, env_var_name);
        #else
        char *lvl = getenv(env_var_name);
        read = (lvl)?1:0;
        #endif
        if (read) {
            return {std::string(lvl), true};
        }
    }
    return {"", false};
}

bool use_debug_feature() {
    std::string value;
    bool read;
    std::tie(value, read) = get_env_variable("HL_MULTITARGET_TEST_USE_DEBUG_FEATURE");
    if (!read) {
        return false;
    }
    return std::stoi(value) != 0;
}

static std::atomic<int> can_use_count;

int my_can_use_target_features(uint64_t features) {
    can_use_count += 1;
    if (features & (1ULL << halide_target_feature_debug)) {
        if (use_debug_feature()) {
            return 1;
        } else {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    const int W = 32, H = 32;
    Buffer<uint32_t> output(W, H);

    halide_set_error_handler(my_error_handler);
    halide_set_custom_can_use_target_features(my_can_use_target_features);

    if (HalideTest::multitarget(output) != 0) {
        printf("Error at multitarget\n");
    }

    // Verify output.
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const uint32_t expected = use_debug_feature() ? 0xdeadbeef : 0xf00dcafe;
            const uint32_t actual = output(x, y);
            if (actual != expected) {
                printf("Error at %d, %d: expected %x, got %x\n", x, y, expected, actual);
                return -1;
            }
        }
    }

    // halide_can_use_target_features() should be called exactly once, with the
    // result cached; call this a few more times to verify.
    for (int i = 0; i < 10; ++i) {
        if (HalideTest::multitarget(output) != 0) {
            printf("Error at multitarget\n");
        }
    }
    if (can_use_count != 1) {
        printf("Error: halide_can_use_target_features was called %d times!\n", (int) can_use_count);
        return -1;
    }

    {
        // Verify that the multitarget wrapper code propagates nonzero error
        // results back to the caller properly.
        Buffer<uint8_t> bad_elem_size(W, H);
        int result = HalideTest::multitarget(bad_elem_size);
        if (result != halide_error_code_bad_elem_size) {
            printf("Error: expected to fail with halide_error_code_bad_elem_size (%d) but actually got %d!\n", (int) halide_error_code_bad_elem_size, result);
            return -1;
        }
    }

    printf("Success: Saw %x for debug=%d\n", output(0, 0), use_debug_feature());

    return 0;
}
