//
// Created by niooi on 8/5/25.
//

#include <engine/graph.h>
#include <prelude.h>

#include <queue>

void TaskGraph::rebuild_graph()
{
    sorted_tasks_.clear();

    if (registered_tasks_.empty())
    {
        return;
    }

    // Build adjacency list and in-degree counts
    v::ud_map<std::string, std::vector<std::string>> graph;
    v::ud_map<std::string, u32>                      in_degree;

    // Initialize all tasks
    for (const auto& [name, def] : registered_tasks_)
    {
        graph[name]; // Ensure node exists
        in_degree[name]; // Initialize in-degree to 0
    }

    // Build edges from dependencies
    for (const auto& [name, def] : registered_tasks_)
    {
        // "after" means dependencies -> this task depends on them
        for (const std::string& dep_name : def.after)
        {
            if (registered_tasks_.contains(dep_name))
            {
                graph[dep_name].push_back(name);
                in_degree[name]++;
            }
        }

        // "before" means this task should run before these tasks
        for (const std::string& succ_name : def.before)
        {
            if (registered_tasks_.contains(succ_name))
            {
                graph[name].push_back(succ_name);
                in_degree[succ_name]++;
            }
        }
    }

    // Kahn's algo for topological sort
    std::queue<std::string> queue;

    // Start with nodes that have no dependencies
    for (const auto& [name, degree] : in_degree)
    {
        if (degree == 0)
        {
            queue.push(name);
        }
    }

    while (!queue.empty())
    {
        std::string current = queue.front();
        queue.pop();
        sorted_tasks_.push_back(current);

        // Reduce in-degree for neighbors
        for (const std::string& neighbor : graph[current])
        {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0)
            {
                queue.push(neighbor);
            }
        }
    }

    // If sorted_tasks_ doesn't contain all tasks, there's a cycle
    if (sorted_tasks_.size() != registered_tasks_.size())
    {
        // Handle cycle: clear sorted tasks to prevent execution
        sorted_tasks_.clear();
    }
}

void TaskGraph::execute()
{
    for (const std::string& task_name : sorted_tasks_)
    {
        const auto& task_def = registered_tasks_.at(task_name);
        task_def.func();
    }
}
