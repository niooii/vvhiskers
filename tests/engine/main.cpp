// Engine core integration tests

#include <engine/context.h>
#include <engine/domain.h>
#include <engine/engine.h>
#include <test.h>
#include <time/time.h>

using namespace v;

// Test context for context management
class TestContext : public Context<TestContext> {
public:
    explicit TestContext(Engine& engine) : Context(engine) {}
    int value = 42;
};

// Test domain for domain management
class TestDomain : public Domain<TestDomain> {
public:
    TestDomain(std::string name = "TestDomain") : Domain(std::move(name)) {}

    int  counter = 0;
    void update() { counter++; }
};

// Test singleton domain
class TestSingletonDomain : public SDomain<TestSingletonDomain> {
public:
    TestSingletonDomain(std::string name = "TestSingletonDomain") :
        SDomain(std::move(name))
    {}
    std::string data = "singleton";
};

int main()
{
    auto [engine, tctx] = testing::init_test("engine");

    // Test basic engine properties
    {
        tctx.assert_now(engine->current_tick() == 0, "Engine starts at tick 0");
        tctx.assert_now(engine->delta_time() == 0.0, "Initial delta time is 0");
        tctx.assert_now(
            engine->is_valid_entity(engine->entity()), "Engine entity is valid");
    }

    // Test engine tick functionality
    {
        u64 initial_tick = engine->current_tick();
        engine->tick();
        tctx.assert_now(
            engine->current_tick() == initial_tick + 1, "Tick counter increments");
        tctx.assert_now(
            engine->delta_time() >= 0.0, "Delta time is non-negative after first tick");

        // Sleep a bit and tick again to test delta time
        v::time::sleep_ms(10);
        engine->tick();
        tctx.assert_now(
            engine->current_tick() == initial_tick + 2, "Tick counter increments again");
        tctx.assert_now(engine->delta_time() > 0.0, "Delta time is positive after sleep");
    }

    // Test context management
    {
        auto* ctx = engine->add_ctx<TestContext>();
        tctx.assert_now(ctx != nullptr, "Context added successfully");
        tctx.assert_now(ctx->value == 42, "Context has expected initial value");

        auto* retrieved_ctx = engine->get_ctx<TestContext>();
        tctx.assert_now(retrieved_ctx == ctx, "Retrieved context is the same instance");
        tctx.assert_now(retrieved_ctx->value == 42, "Retrieved context maintains state");

        // Test context modification
        ctx->value         = 100;
        auto* modified_ctx = engine->get_ctx<TestContext>();
        tctx.assert_now(modified_ctx->value == 100, "Context state changes persist");
    }

    // Test duplicate context handling
    {
        auto* ctx1  = engine->add_ctx<TestContext>();
        ctx1->value = 200;

        auto* ctx2 = engine->add_ctx<TestContext>();
        tctx.assert_now(ctx2 != nullptr, "Duplicate context replacement allowed");
        tctx.assert_now(ctx2->value == 42, "New context has default value");

        auto* retrieved = engine->get_ctx<TestContext>();
        tctx.assert_now(retrieved == ctx2, "New context replaces old one");
        tctx.assert_now(retrieved->value == 42, "Retrieved context is the new one");
    }

    // Test domain management
    {
        auto& domain = engine->add_domain<TestDomain>();
        tctx.assert_now(domain.counter == 0, "Domain has expected initial counter");

        auto* retrieved_domain = engine->get_domain<TestDomain>();
        tctx.assert_now(
            retrieved_domain == &domain, "Retrieved domain is the same instance");

        // Test domain entity
        tctx.assert_now(
            engine->is_valid_entity(domain.entity()), "Domain entity is valid");
    }

    // Test multiple domains
    {
        auto& domain1 = engine->add_domain<TestDomain>("Domain1");
        auto& domain2 = engine->add_domain<TestDomain>("Domain2");

        tctx.assert_now(&domain1 != &domain2, "Multiple domains are different instances");
        tctx.assert_now(domain1.name() == "Domain1", "Domain1 has correct name");
        tctx.assert_now(domain2.name() == "Domain2", "Domain2 has correct name");

        // Test view functionality
        auto view = engine->view<TestDomain>();
        tctx.assert_now(view.size() >= 2, "View contains multiple domains");

        int found_count = 0;
        for (auto [entity, domain] : view.each())
        {
            found_count++;
            tctx.assert_now(
                domain->name() == "Domain1" || domain->name() == "Domain2" ||
                    domain->name() == "TestDomain",
                "Found domain has expected name");
        }
        tctx.assert_now(found_count >= 2, "Found expected number of domains");
    }

    // Test singleton domain behavior
    {
        auto& singleton1 = engine->add_domain<TestSingletonDomain>();

        auto& singleton2 = engine->add_domain<TestSingletonDomain>();
        tctx.assert_now(
            &singleton2 == &singleton1, "Singleton domain returns existing instance");

        auto* retrieved = engine->get_domain<TestSingletonDomain>();
        tctx.assert_now(
            retrieved == &singleton1, "Retrieved singleton is the same instance");
    }

    // Test component management on engine entity
    {
        auto engine_entity = engine->entity();

        tctx.assert_now(
            !engine->has_component<int>(engine_entity),
            "Engine entity doesn't have test component initially");

        int& comp = engine->add_component<int>(engine_entity, 123);
        tctx.assert_now(comp == 123, "Component added with correct value");
        tctx.assert_now(
            engine->has_component<int>(engine_entity),
            "Engine entity has component after adding");

        int* retrieved = engine->try_get_component<int>(engine_entity);
        tctx.assert_now(retrieved != nullptr, "Component retrieved successfully");
        tctx.assert_now(*retrieved == 123, "Retrieved component has correct value");

        int& retrieved_ref = engine->get_component<int>(engine_entity);
        tctx.assert_now(retrieved_ref == 123, "Component reference has correct value");

        size_t removed = engine->remove_component<int>(engine_entity);
        tctx.assert_now(removed == 1, "Component removed successfully");
        tctx.assert_now(
            !engine->has_component<int>(engine_entity),
            "Component no longer exists after removal");
    }

    // Test post_tick functionality
    {
        bool post_tick_executed = false;
        engine->post_tick([&post_tick_executed]() { post_tick_executed = true; });

        tctx.assert_now(
            !post_tick_executed, "Post tick callback not executed immediately");

        engine->tick();
        tctx.assert_now(post_tick_executed, "Post tick callback executed after tick");
    }

    // Test on_tick callbacks
    {
        int tick_count = 0;
        engine->on_tick.connect({}, {}, "test_tick", [&tick_count]() { tick_count++; });

        engine->tick();
        tctx.assert_now(tick_count == 1, "Tick callback executed once");

        engine->tick();
        tctx.assert_now(tick_count == 2, "Tick callback executed twice");

        // Clean up the callback before tick_count goes out of scope
        engine->on_tick.disconnect("test_tick");
    }

    // Test domain lifecycle management
    {
        auto& domain        = engine->add_domain<TestDomain>("LifecycleTest");
        auto  domain_entity = domain.entity();

        tctx.assert_now(
            engine->is_valid_entity(domain_entity), "Domain entity is valid initially");

        engine->queue_destroy_domain(domain_entity);
        tctx.assert_now(
            engine->is_valid_entity(domain_entity),
            "Domain entity still valid before tick");

        engine->tick(); // This should execute the post_tick destroy
        tctx.assert_now(
            !engine->is_valid_entity(domain_entity),
            "Domain entity destroyed after tick");
    }

    return tctx.is_failure();
}
