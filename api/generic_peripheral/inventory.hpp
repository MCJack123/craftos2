/*
 * generic_peripheral/inventory.hpp
 * CraftOS-PC 2
 * 
 * This file defines a generic peripheral class for inventory-type peripherals
 * to inherit from.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#ifndef CRAFTOS_PC_GENERIC_PERIPHERAL_INVENTORY_HPP
#define CRAFTOS_PC_GENERIC_PERIPHERAL_INVENTORY_HPP
#include "../Computer.hpp"
#include "../peripheral.hpp"

class inventory : public peripheral {
protected:
    virtual int size() = 0; // Returns the number of slots available in the inventory.
    virtual int getItemSpace(int slot) {return 64;} // Returns the maximum number of items in a slot (defaults to 64 for all).
    virtual void getItemDetail(lua_State *L, int slot) = 0; // Pushes a single Lua value to the stack with details about an item in a slot. If the slot is empty or invalid, pushes nil. (Slots are in the range 1-size().)
    virtual int addItems(lua_State *L, int slot, int count) = 0; // Adds the item described at the top of the Lua stack to the slot selected. Only adds up to count items. If slot is 0, determine the best slot available. Returns the number of items added.
    virtual int removeItems(int slot, int count) = 0; // Removes up to a number of items from the selected slot. Returns the number of items removed.
public:
    virtual int call(lua_State *L, const char * method) override {
        const std::string m(method);
        if (m == "size") {
            lua_pushinteger(L, size());
            return 1;
        } else if (m == "list") {
            lua_createtable(L, size(), 0);
            for (int i = 1; i <= size(); i++) {
                lua_pushinteger(L, i);
                lua_createtable(L, 0, 3);
                getItemDetail(L, i);
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "name");
                    lua_setfield(L, -3, "name");
                    lua_getfield(L, -1, "count");
                    lua_setfield(L, -3, "count");
                    lua_getfield(L, -1, "nbt");
                    lua_setfield(L, -3, "nbt");
                    lua_pop(L, 1);
                } else {
                    lua_remove(L, -2);
                }
                lua_settable(L, -3);
            }
            return 1;
        } else if (m == "getItemDetail") {
            getItemDetail(L, luaL_checkinteger(L, 1));
            return 1;
        } else if (m == "getItemLimit") {
            lua_pushinteger(L, getItemSpace(luaL_checkinteger(L, 1)));
            return 1;
        } else if (m == "pushItems" || m == "pullItems") {
            Computer * comp = get_comp(L);
            const char * side = luaL_checkstring(L, 1);
            const int fromSlot = luaL_checkinteger(L, 2);
            const int limit = luaL_optinteger(L, 3, INT_MAX);
            const int toSlot = luaL_optinteger(L, 4, 0);

            if (comp->peripherals.find(side) == comp->peripherals.end()) return luaL_error(L, "Target '%s' does not exist", side);
            inventory * p = dynamic_cast<inventory*>(comp->peripherals[side]);
            if (p == NULL) return luaL_error(L, "Target '%s' is not an inventory", side);
            inventory *src, *dest;
            if (m == "pushItems") src = this, dest = p;
            else src = p, dest = this;

            if (fromSlot < 1 || fromSlot > src->size()) return luaL_error(L, "From slot out of range (between 1 and %d)", src->size());
            if (!lua_isnil(L, 4) && (toSlot < 1 || toSlot > dest->size())) return luaL_error(L, "To slot out of range (between 1 and %d)", dest->size());
            if (limit <= 0) {
                lua_pushinteger(L, 0);
                return 1;
            }

            src->getItemDetail(L, fromSlot);
            const int removed = src->removeItems(fromSlot, limit);
            if (removed == 0) {
                lua_pushinteger(L, 0);
                return 1;
            }
            const int added = dest->addItems(L, toSlot, removed);
            if (added < removed) src->addItems(L, fromSlot, removed - added); // hopefully this will still be OK

            lua_pushinteger(L, added);
            return 1;
        } else return luaL_error(L, "No such method");
    }
    void update() override {}
    virtual library_t getMethods() const override {
        static luaL_Reg reg[] = {
            {"size", NULL},
            {"list", NULL},
            {"getItemDetail", NULL},
            {"getItemLimit", NULL},
            {"pushItems", NULL},
            {"pullItems", NULL},
            {NULL, NULL}
        };
        static library_t methods = {"inventory", reg, nullptr, nullptr};
        return methods;
    }
};

#endif