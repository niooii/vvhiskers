#include <test.h>
#include "testdomain.h"

using namespace v;

int main()
{
    auto [engine, tctx] = testing::init_test("domain");

    // Create 8 example CountTo10Domain instances
    for (i32 i = 0; i < 8; ++i)
    {
        engine->add_domain<CountTo10Domain>("CountTo10Domain_" + std::to_string(i));
    }

    // Verify all domains were created
    auto initial_count = engine->view<CountTo10Domain>().size();
    tctx.assert_now(initial_count == 8, "8 domains created");

    bool all_domains_updated = false;
    u64  domain_count_zero   = 0;

    // Set up domain update loop - CountTo10Domain counts to 10, then destroys itself
    engine->on_tick.connect(
        {}, {}, "domain updates",
        [&]()
        {
            for (auto [entity, domain] : engine->view<CountTo10Domain>().each())
            {
                domain->update();
            }

            // Check if all domains have self-destructed
            auto current_count = engine->view<CountTo10Domain>().size();
            if (current_count == 0)
            {
                domain_count_zero++;
                if (domain_count_zero >= 3) // stable for a few ticks
                    all_domains_updated = true;
            }
        });

    constexpr u64 max_ticks = 2000; // test budget
    for (u64 i = 0; i < max_ticks; ++i)
    {
        engine->tick();

        // Progress check - domains should eventually self-destruct
        tctx.expect_before(all_domains_updated, 1500, "all domains self-destructed");

        if (all_domains_updated)
            break;
    }

    // Final hard assert
    tctx.assert_now(all_domains_updated, "domains completed lifecycle");
    tctx.assert_now(engine->view<CountTo10Domain>().size() == 0, "no domains remain");

    return tctx.is_failure();
}
