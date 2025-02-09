// Copyright (c) 2019-2024 JackMacWindows.
// SPDX-FileCopyrightText: 2019-2024 JackMacWindows
//
// SPDX-License-Identifier: MIT

#ifndef PERIPHERAL_CHEST_HPP
#define PERIPHERAL_CHEST_HPP
#include "../util.hpp"
#include <generic_peripheral/inventory.hpp>

class chest: public inventory {
    struct slot {
        std::string name;
        uint8_t count = 0;
    };
    bool isDouble = false;
    slot items[54];
    int setItem(lua_State *L);
protected:
    int size() override;
    void getItemDetail(lua_State *L, int slot) override;
    int addItems(lua_State *L, int slot, int count) override;
    int removeItems(int slot, int count) override;
public:
    static library_t methods;
    static std::vector<std::string> types;
    static peripheral * init(lua_State *L, const char * side) {return new chest(L, side);}
    static void deinit(peripheral * p) {delete (chest*)p;}
    destructor getDestructor() const override {return deinit;}
    library_t getMethods() const override {return methods;}
    std::vector<std::string> getTypes() const override {return types;}
    chest(lua_State *L, const char * side);
    ~chest();
    int call(lua_State *L, const char * method) override;
};

#endif
