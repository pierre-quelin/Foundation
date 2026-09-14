/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Register.hpp
 * @brief Static registration macro and link anchor for static-library builds.
 *
 * Place FOUNDATION_FACTORY_REGISTER in the .cpp of each concrete factory type.
 * The application template calls link::touch_*() from FactoryLink.cpp so the
 * linker pulls the object file out of libfoundation.a (see plan-foundation-phase2.md).
 */
#pragma once

#include "tools/design/factory/ILaunchable.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/factory/Registry.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace tools::design::factory
{

/**
 * @brief Registers a creator; Registrar::touch exposes a stable link symbol.
 */
template <typename T>
class Registrar
{
public:
    explicit Registrar(std::string name) : _name(std::move(name))
    {
        static_assert(std::is_base_of_v<IObject, T>,
                      "FOUNDATION_FACTORY_REGISTER type must derive from factory::IObject");

        Registry::instance().add(
            _name,
            [](ApplicationServices& app, config::Node& node) -> CreatedObject
            {
                auto typed              = std::shared_ptr<T>(new T(app, node));
                IObject* const asObject = static_cast<IObject*>(typed.get());
                return CreatedObject{std::shared_ptr<void>(typed),
                                     asObject,
                                     dynamic_cast<ILaunchable*>(asObject)};
            });
    }

private:
    std::string _name;
};

namespace link
{

template <typename T>
const Registrar<T>& registrarFor(std::string_view name)
{
    static const Registrar<T> instance{std::string{name}};
    return instance;
}

} // namespace link

} // namespace tools::design::factory

/**
 * @param Type   Concrete class — **public** constructor: `(ApplicationServices&, config::Node)` only.
 *               Implementation constructors (injection, USB id, …) stay **private** in the `.cpp` header.
 *               Must derive from @c tools::design::factory::IObject.
 * @param Name   InstanceOf string in JSON (e.g. "tools::os::serport::Serport").
 * @param LinkId C identifier for link::touch_<LinkId>() — fully-qualified type with `::` → `_`
 *             (e.g. `tools::os::serport::Serport` → `tools_os_serport_Serport`).
 */
#define FOUNDATION_FACTORY_REGISTER(Type, Name, LinkId)                     \
    namespace tools::design::factory::link                                  \
    {                                                                       \
    [[maybe_unused]] static const ::tools::design::factory::Registrar<Type> \
        s_registrar_##LinkId{Name};                                         \
    void touch_##LinkId()                                                   \
    {                                                                       \
        (void)s_registrar_##LinkId;                                         \
    }                                                                       \
    }
