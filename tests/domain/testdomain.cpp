//
// Created by niooi on 11/23/2025.
//

#include "testdomain.h"
#include <engine/engine.h>

namespace v {
    CountTo10Domain::CountTo10Domain(const std::string& name) :
        Domain<CountTo10Domain>(name), counter_(1)
    {}

    void CountTo10Domain::update()
    {
        if (counter_ <= 10)
        {
            counter_++;
        }

        if (counter_ > 10)
        {
            const entt::entity id = entity_;
            engine().post_tick(
                [this, id]()
                {
                    if (engine().registry().valid(id))
                        engine().registry().destroy(id);
                });
        }
    }
} // namespace v
