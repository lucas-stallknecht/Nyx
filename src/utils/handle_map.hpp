#pragma once

#include "types.hpp"
#include <array>

template <typename T>
concept HasHandle = requires(T t) {
    { t.handle } -> std::same_as<Handle &>;
};

template <typename T, usize N>
    requires HasHandle<T>
struct HandleMap
{
    std::array<T, N> items = {};
    usize items_count = 0;
    std::vector<usize> free_list = {};

    Handle insert(T value)
    {
        assert((items_count < N - 1 || !free_list.empty()) && "HandleMap is full");

        // Allocate sentinel
        if (items_count == 0)
        {
            items = {};
            items_count = 1;
        }

        // Find next available slot
        usize new_idx;
        if (!free_list.empty())
        {
            new_idx = free_list.back();
            free_list.pop_back();
        }
        else
        {
            new_idx = items_count++;
        }

        T & item = items[new_idx];

        usize curr_gen = item.handle.gen;

        item = std::move(value);
        item.handle = {new_idx, std::max<usize>(1, curr_gen)};

        return item.handle;
    }

    void erase(Handle handle)
    {
        usize idx = handle.idx;
        usize gen = handle.gen;

        if (idx == 0 || idx >= items_count)
        {
            return;
        }

        T & item = items[idx];

        // Stale handle will now fail
        if (item.handle == handle)
        {
            item.handle.idx = 0;
            item.handle.gen = gen + 1;

            free_list.push_back(idx);
        }
    }

    T * get(Handle handle)
    {
        usize idx = handle.idx;

        if (idx == 0 || idx >= items_count)
        {
            return nullptr;
        }

        T & item = items[idx];
        if (item.handle == handle)
        {
            return &item;
        }

        return nullptr;
    }

    bool valid(Handle handle)
    {
        usize idx = handle.idx;
        if (!handle.valid() || idx >= items_count)
        {
            return false;
        }

        return items[idx].handle == handle;
    }
};
