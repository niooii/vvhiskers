//
// Created by niooi on 8/3/25.
//

#pragma once

#include <containers/ud_map.h>
#include <defs.h>
#include <functional>
#include <string>
#include <vector>

struct TaskDefinition {
    std::string              name;
    std::function<void()>    func;
    std::vector<std::string> after; // Tasks this one should run AFTER
    std::vector<std::string> before; // Tasks this one should run BEFORE
};

/// Task dependency manager
class TaskGraph {
public:
    /// Connect a task with dependency specifications
    /// @param after Tasks that this task should run AFTER
    /// @param before Tasks that this task should run BEFORE
    /// @param name Unique name for this task
    /// @param func Function to execute
    /// @note ALL tasks should be thread-safe
    /// TODO! why doesnt this just take in a task definition?
    FORCEINLINE void connect(
        const std::vector<std::string>& after, const std::vector<std::string>& before,
        const std::string& name, std::function<void()> func)
    {
        TaskDefinition task_def;
        task_def.name   = name;
        task_def.func   = std::move(func);
        task_def.after  = after;
        task_def.before = before;

        registered_tasks_[name] = std::move(task_def);
        rebuild_graph();
    }

    /// Disconnect a task by name
    /// @param name Name of the task to remove
    FORCEINLINE void disconnect(const std::string& name)
    {
        registered_tasks_.erase(name);
        rebuild_graph();
    }

    /// Execute the task graph
    void execute();

private:
    /// Rebuild the topological order when tasks are added or removed
    void rebuild_graph();

    v::ud_map<std::string, TaskDefinition> registered_tasks_;
    std::vector<std::string>               sorted_tasks_;
};
