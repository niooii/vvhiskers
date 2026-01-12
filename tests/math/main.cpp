// Math utilities integration tests

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <test.h>
#include <vmath.h>

using namespace v;

int main()
{
    auto [engine, tctx] = testing::init_test("math");

    // Test vector clamp with scalar bounds (GLM should handle this via vec1)
    {
        glm::vec3 v(-2.0f, 0.5f, 2.5f);
        glm::vec3 clamped = v::clamp(v, 0.0f, 1.0f);

        tctx.assert_now(clamped.x == 0.0f, "clamp min bound");
        tctx.assert_now(clamped.y == 0.5f, "clamp within bounds");
        tctx.assert_now(clamped.z == 1.0f, "clamp max bound");
    }

    // Test vector clamp with vector bounds
    {
        glm::vec3 v(0.5f, 1.5f, 2.5f);
        glm::vec3 lo(0.0f, 1.0f, 2.0f);
        glm::vec3 hi(1.0f, 2.0f, 3.0f);
        glm::vec3 clamped = v::clamp(v, lo, hi);

        tctx.assert_now(clamped.x == 0.5f, "component-wise clamp x");
        tctx.assert_now(clamped.y == 1.5f, "component-wise clamp y");
        tctx.assert_now(clamped.z == 2.5f, "component-wise clamp z");
    }

    // Test saturate
    {
        glm::vec4 v(-0.5f, 0.0f, 0.5f, 1.5f);
        glm::vec4 sat = v::saturate(v);

        tctx.assert_now(sat.x == 0.0f, "saturate negative");
        tctx.assert_now(sat.y == 0.0f, "saturate zero");
        tctx.assert_now(sat.z == 0.5f, "saturate within range");
        tctx.assert_now(sat.w == 1.0f, "saturate above one");
    }

    // Test max_component and min_component
    {
        glm::vec3 v(3.0f, 1.0f, 5.0f);

        tctx.assert_now(v::max_component(v) == 5.0f, "max_component correct");
        tctx.assert_now(v::min_component(v) == 1.0f, "min_component correct");
    }

    // Test vector pow with scalar exponent
    {
        glm::vec2 v(2.0f, 3.0f);
        glm::vec2 result = v::pow(v, 2.0f);

        tctx.assert_now(std::abs(result.x - 4.0f) < 0.001f, "pow scalar exponent x");
        tctx.assert_now(std::abs(result.y - 9.0f) < 0.001f, "pow scalar exponent y");
    }

    // Test vector pow with vector exponent
    {
        glm::vec2 v(2.0f, 3.0f);
        glm::vec2 exp(3.0f, 2.0f);
        glm::vec2 result = v::pow(v, exp);

        tctx.assert_now(std::abs(result.x - 8.0f) < 0.001f, "pow vector exponent x");
        tctx.assert_now(std::abs(result.y - 9.0f) < 0.001f, "pow vector exponent y");
    }

    // Test integer power (ipow)
    {
        glm::ivec3 v(2, 3, 4);
        glm::ivec3 result = v::ipow(v, 3);

        tctx.assert_now(result.x == 8, "ipow 2^3");
        tctx.assert_now(result.y == 27, "ipow 3^3");
        tctx.assert_now(result.z == 64, "ipow 4^3");
    }

    // Test floor_log and ceil_log
    {
        tctx.assert_now(v::floor_log(8.0, 2.0) == 3, "floor_log(8, 2)");
        tctx.assert_now(v::ceil_log(8.0, 2.0) == 3, "ceil_log(8, 2)");
        tctx.assert_now(v::floor_log(9.0, 2.0) == 3, "floor_log(9, 2)");
        tctx.assert_now(v::ceil_log(9.0, 2.0) == 4, "ceil_log(9, 2)");
    }

    // Test integer log functions
    {
        tctx.assert_now(v::ifloor_log(8u, 2u) == 3, "ifloor_log(8, 2)");
        tctx.assert_now(v::iceil_log(8u, 2u) == 3, "iceil_log(8, 2)");
        tctx.assert_now(v::ifloor_log(9u, 2u) == 3, "ifloor_log(9, 2)");
        tctx.assert_now(v::iceil_log(9u, 2u) == 4, "iceil_log(9, 2)");

        // Edge cases
        tctx.assert_now(v::ifloor_log(1u, 2u) == 0, "ifloor_log(1, 2)");
        tctx.assert_now(v::iceil_log(1u, 2u) == 0, "iceil_log(1, 2)");
    }

    // Test power of 2 log functions
    {
        tctx.assert_now(v::floor_log_pow2(8u, 1) == 3, "floor_log_pow2(8, 1)");
        tctx.assert_now(v::ceil_log_pow2(8u, 1) == 3, "ceil_log_pow2(8, 1)");
        tctx.assert_now(v::floor_log_pow2(16u, 2) == 2, "floor_log_pow2(16, 2)");
        tctx.assert_now(v::ceil_log_pow2(16u, 2) == 2, "ceil_log_pow2(16, 2)");
    }

    // Test error cases
    {
        tctx.assert_now(
            v::floor_log(0.0, 2.0) == std::numeric_limits<i32>::min(),
            "floor_log error case");
        tctx.assert_now(
            v::ceil_log(0.0, 2.0) == std::numeric_limits<i32>::min(),
            "ceil_log error case");
        tctx.assert_now(
            v::ifloor_log(0u, 2u) == std::numeric_limits<i32>::min(),
            "ifloor_log error case");
        tctx.assert_now(
            v::floor_log_pow2(0u, 1) == std::numeric_limits<i32>::min(),
            "floor_log_pow2 error case");
    }

    return tctx.is_failure();
}
