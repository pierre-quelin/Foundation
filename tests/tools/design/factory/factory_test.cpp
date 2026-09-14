/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/factory/Factory.hpp"

#include "tools/design/config/Config.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/BootRoots.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/factory/Registry.hpp"
#include "tools/design/factory/Tree.hpp"
#include "tools/os/serport/Serport.h"
#include "util/logger/Logger.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;

namespace tools::design::test
{

/** Interface cible — équivalent io::in::IIn dans les tests factory. */
class IHello
{
public:
    virtual ~IHello() = default;

    [[nodiscard]] virtual int param() const = 0;
};

/** Implémentation « prod » — équivalent io::in::InByPCA9539. */
class HelloObject : public factory::IObject, public IHello
{
public:
    HelloObject(ApplicationServices& app, config::Node node) : _param(node["Param"].value<int>())
    {
        (void)app;
    }

    [[nodiscard]] int param() const override { return _param; }

private:
    int _param;
};

/** Mock banc — enregistré uniquement dans les tests. */
class HelloMock : public factory::IObject, public IHello
{
public:
    HelloMock(ApplicationServices& app, config::Node node) : _value(node.contains("Default") ? node["Default"].value<unsigned>() : 0u)
    {
        (void)app;
    }

    [[nodiscard]] int param() const override { return static_cast<int>(_value); }

    void set(unsigned value) { _value = value; }

private:
    unsigned _value;
};

/** Parent wired via Objects + Item list (prod pattern). */
class HelloParent : public factory::IObject, public IHello
{
public:
    HelloParent(ApplicationServices& app, config::Node node)
    {
        createUniqueFromItemList<HelloObject>(app, node, "Children", _children);
        for (const auto& [name, child] : _children)
        {
            (void)name;
            _sum += child->param();
        }
    }

    [[nodiscard]] int param() const override { return _sum; }

    [[nodiscard]] const HelloObject* child(std::string_view name) const
    {
        const auto it = _children.find(std::string{name});
        return it != _children.end() ? it->second.get() : nullptr;
    }

private:
    std::unordered_map<std::string, factory::OwnedPtr<HelloObject>> _children;
    int _sum = 0;
};

/** Tracks destructor order for destroyGlobalObjects tests. */
inline std::vector<std::string>& destroyProbeOrder()
{
    static std::vector<std::string> order;
    return order;
}

class DestroyProbe : public factory::IObject
{
public:
    DestroyProbe(ApplicationServices& app, config::Node node) : _name(config::instanceName(node.path()))
    {
        (void)app;
    }

    ~DestroyProbe() override { destroyProbeOrder().push_back(_name); }

private:
    std::string _name;
};

class LaunchProbe : public factory::IObject, public factory::ILaunchable
{
public:
    LaunchProbe(ApplicationServices& app, config::Node node)
    {
        (void)app;
        (void)node;
    }

    void launch() override { ++launchCount(); }

    [[nodiscard]] static int& launchCount()
    {
        static int count = 0;
        return count;
    }
};

} // namespace tools::design::test

FOUNDATION_FACTORY_REGISTER(tools::design::test::HelloObject,
                            "tools::design::test::HelloObject",
                            tools_design_test_HelloObject)

FOUNDATION_FACTORY_REGISTER(tools::design::test::DestroyProbe,
                            "tools::design::test::DestroyProbe",
                            tools_design_test_DestroyProbe)

FOUNDATION_FACTORY_REGISTER(tools::design::test::LaunchProbe,
                            "tools::design::test::LaunchProbe",
                            tools_design_test_LaunchProbe)

FOUNDATION_FACTORY_REGISTER(tools::design::test::HelloMock,
                            "tools::design::test::HelloMock",
                            tools_design_test_HelloMock)

FOUNDATION_FACTORY_REGISTER(tools::design::test::HelloParent,
                            "tools::design::test::HelloParent",
                            tools_design_test_HelloParent)

namespace
{

constexpr const char* HelloConfigJson = R"({
  "Hello": {
    "InstanceOf": "tools::design::test::HelloObject",
    "Param": 7
  }
})";

/** JSON identique à la prod : InstanceOf = type concret (HelloObject). */
constexpr const char* ProdLikeHelloJson = R"({
  "MyHello": {
    "InstanceOf": "tools::design::test::HelloObject",
    "Default": 42
  }
})";

constexpr const char* HelloTreeJson = R"({
  "Parent": {
    "InstanceOf": "tools::design::test::HelloParent",
    "Children": [
      { "Item": "first" },
      { "Item": "second" }
    ],
    "Objects": {
      "first": {
        "InstanceOf": "tools::design::test::HelloObject",
        "Param": 3
      },
      "second": {
        "InstanceOf": "tools::design::test::HelloObject",
        "Param": 4
      }
    }
  }
})";

} // namespace

BOOST_AUTO_TEST_CASE(factory_create_from_instance_of)
{
    const auto center = createLocalFromString(HelloConfigJson);
    ApplicationServices app;
    app.config = center;

    auto object = create<test::HelloObject>(app, center->root()["Hello"]);
    BOOST_REQUIRE(object);
    BOOST_CHECK_EQUAL(object->param(), 7);
}

BOOST_AUTO_TEST_CASE(factory_create_named_children_tree)
{
    const auto center = createLocalFromString(HelloTreeJson);
    ApplicationServices app;
    app.config = center;

    const auto names = itemNames(center->root()["Parent"]["Children"]);
    BOOST_REQUIRE_EQUAL(names.size(), 2u);
    BOOST_CHECK_EQUAL(names[0], "first");
    BOOST_CHECK_EQUAL(names[1], "second");

    auto parent = create<test::HelloParent>(app, center->root()["Parent"]);
    BOOST_REQUIRE(parent);
    BOOST_CHECK_EQUAL(parent->param(), 7);
    BOOST_REQUIRE(parent->child("first"));
    BOOST_CHECK_EQUAL(parent->child("first")->param(), 3);
    BOOST_REQUIRE(parent->child("second"));
    BOOST_CHECK_EQUAL(parent->child("second")->param(), 4);
}

BOOST_AUTO_TEST_CASE(factory_item_names)
{
    constexpr const char* json = R"({
      "PCA9633s": [
        { "Item": "PCA9633_0" },
        { "Item": "PCA9633_1" }
      ]
    })";
    const auto center          = createLocalFromString(json);
    const auto names           = itemNames(center->root()["PCA9633s"]);
    BOOST_REQUIRE_EQUAL(names.size(), 2u);
    BOOST_CHECK_EQUAL(names[0], "PCA9633_0");
    BOOST_CHECK_EQUAL(names[1], "PCA9633_1");
}

BOOST_AUTO_TEST_CASE(factory_create_shared_from_item_list)
{
    const auto center = createLocalFromString(HelloTreeJson);
    ApplicationServices app;
    app.config = center;

    std::unordered_map<std::string, std::shared_ptr<test::HelloObject>> children;
    createSharedFromItemList<test::HelloObject>(app, center->root()["Parent"], "Children", children);
    BOOST_REQUIRE_EQUAL(children.size(), 2u);
    BOOST_CHECK_EQUAL(children.at("first")->param(), 3);
    BOOST_CHECK_EQUAL(children.at("second")->param(), 4);
}

BOOST_AUTO_TEST_CASE(factory_create_by_name_from_objects)
{
    constexpr const char* json = R"({
      "Board": {
        "InstanceOf": "tools::design::test::HelloParent",
        "Child": "first",
        "Objects": {
          "first": {
            "InstanceOf": "tools::design::test::HelloObject",
            "Param": 11
          }
        }
      }
    })";
    const auto center          = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;

    auto object = create<test::HelloObject>(app, center->root()["Board"], "first");
    BOOST_REQUIRE(object);
    BOOST_CHECK_EQUAL(object->param(), 11);
}

BOOST_AUTO_TEST_CASE(factory_create_shared_registers_for_obtain)
{
    const auto center = createLocalFromString(HelloConfigJson);
    ApplicationServices app;
    app.config = center;

    const auto created  = createShared<test::HelloObject>(app, center->root()["Hello"]);
    const auto lookedUp = obtain<test::HelloObject>(app, center->root(), "Hello");
    BOOST_REQUIRE(created);
    BOOST_REQUIRE(lookedUp);
    BOOST_CHECK(created.get() == lookedUp.get());
    BOOST_CHECK_EQUAL(created->param(), 7);
}

BOOST_AUTO_TEST_CASE(factory_create_shared_registers_logical_path_without_objects)
{
    const auto center = createLocalFromString(HelloTreeJson);
    ApplicationServices app;
    app.config = center;

    const auto created =
        createShared<test::HelloObject>(app, center->root()["Parent"], "first");
    BOOST_REQUIRE(created);
    BOOST_REQUIRE(app.instances);
    BOOST_CHECK(app.instances->contains("Parent.first"));
    BOOST_CHECK(!app.instances->contains("Parent/Objects/first"));

    const auto lookedUp = obtain<test::HelloObject>(app, center->root()["Parent"], "first");
    BOOST_CHECK(created.get() == lookedUp.get());
}

BOOST_AUTO_TEST_CASE(factory_derived_instance_key_uses_hash_not_dot)
{
    BOOST_CHECK_EQUAL(derivedInstanceKey("IOBoard.pres24v", "bridge"), "IOBoard.pres24v#bridge");
    BOOST_CHECK_EQUAL(derivedInstanceKey("Serport", "bridge"), "Serport#bridge");
}

BOOST_AUTO_TEST_CASE(factory_create_shared_named_and_obtain_without_objects)
{
    constexpr const char* json = R"({
      "Board": {
        "Inputs": [ { "Item": "consumer" } ],
        "Objects": {
          "consumer": {
            "InstanceOf": "tools::design::test::HelloObject",
            "Param": 1,
            "Dep": "Board.chip"
          }
        }
      }
    })";
    const auto center          = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();

    const auto board = center->root()["Board"];
    BOOST_CHECK(!board.contains("Objects") || !board["Objects"].contains("chip"));

    const auto chip =
        createSharedNamed<test::HelloObject>(app, board, "chip", {{"Param", 42}});
    BOOST_REQUIRE(chip);
    BOOST_CHECK_EQUAL(chip->param(), 42);
    BOOST_REQUIRE(app.instances);
    BOOST_CHECK(app.instances->contains("Board.chip"));
    BOOST_CHECK(!center->root().at("Board/Objects").contains("chip"));

    const auto byShort = obtain<test::HelloObject>(app, board, "chip");
    BOOST_CHECK(chip.get() == byShort.get());

    const auto consumerNode = center->root().at("Board/Objects/consumer");
    const auto byDotted     = obtain<test::HelloObject>(app, consumerNode, "Dep");
    BOOST_CHECK(chip.get() == byDotted.get());
}

BOOST_AUTO_TEST_CASE(factory_logger_ptr_uses_short_unique_name)
{
    constexpr const char* json = R"({
      "BoardA": {
        "Objects": {
          "MCP2221": { "InstanceOf": "tools::design::test::HelloObject", "Param": 1 }
        }
      },
      "BoardB": {
        "Objects": {
          "MCP2221": { "InstanceOf": "tools::design::test::HelloObject", "Param": 2 }
        }
      }
    })";
    const auto center          = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    app.logs   = std::make_shared<util::logger::LogService>();

    const auto first  = app.loggerPtr(center->root().at("BoardA/Objects/MCP2221"));
    const auto second = app.loggerPtr(center->root().at("BoardB/Objects/MCP2221"));
    BOOST_REQUIRE(first);
    BOOST_REQUIRE(second);
    BOOST_CHECK_EQUAL(first->name(), "MCP2221");
    BOOST_CHECK_EQUAL(second->name(), "MCP2221~1");
}

BOOST_AUTO_TEST_CASE(factory_obtain_throws_when_missing)
{
    const auto center = createLocalFromString(HelloConfigJson);
    ApplicationServices app;
    app.config = center;

    BOOST_CHECK_THROW((void)obtain<test::HelloObject>(app, center->root(), "Hello"),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(factory_obtain_can_create_if_missing)
{
    const auto center = createLocalFromString(HelloConfigJson);
    ApplicationServices app;
    app.config = center;

    const auto first =
        obtain<test::HelloObject>(app, center->root(), "Hello", /*createIfMissing=*/true);
    const auto second = obtain<test::HelloObject>(app, center->root(), "Hello");
    BOOST_REQUIRE(first);
    BOOST_REQUIRE(second);
    BOOST_CHECK(first.get() == second.get());
    BOOST_CHECK_EQUAL(first->param(), 7);
}

BOOST_AUTO_TEST_CASE(factory_create_shared_rejects_duplicate)
{
    const auto center = createLocalFromString(HelloConfigJson);
    ApplicationServices app;
    app.config = center;

    (void)createShared<test::HelloObject>(app, center->root()["Hello"]);
    BOOST_CHECK_THROW((void)createShared<test::HelloObject>(app, center->root()["Hello"]),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(factory_unknown_type_throws)
{
    constexpr const char* json = R"({"X":{"InstanceOf":"no.such.Type"}})";
    const auto center          = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;

    BOOST_CHECK_THROW(
        [&]
        { (void)createFromNode(app, center->root()["X"]); }(),
        std::runtime_error);
}

/**
 * Cas retenu pour addAlias : JSON de prod inchangé (InstanceOf concret), mock en test.
 * Voir src/tools/design/factory/README.md
 */
BOOST_AUTO_TEST_CASE(factory_alias_redirects_prod_json_to_mock)
{
    Registry::instance().addAlias("tools::design::test::HelloObject",
                                  "tools::design::test::HelloMock");

    const auto center = createLocalFromString(ProdLikeHelloJson);
    ApplicationServices app;
    app.config = center;

    auto object = create<test::IHello>(app, center->root()["MyHello"]);
    BOOST_REQUIRE(object);
    BOOST_CHECK_EQUAL(object->param(), 42);

    Registry::instance().clearAliases();
}

BOOST_AUTO_TEST_CASE(factory_serport_register)
{
    // Factory wiring only — port name need not exist (open failure is tolerated by Serport).
    constexpr const char* json = R"({
      "Port": {
        "InstanceOf": "tools::os::serport::Serport",
        "DeviceName": "__foundation_no_such_port__"
      }
    })";
    const auto center          = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;

    auto port = create<tools::os::serport::Serport>(app, center->root()["Port"]);
    BOOST_REQUIRE(port);
}

/**
 * Opt-in hardware test — not run in CI by default.
 *
 * On a machine with a real serial port, set the device before running utests, e.g.:
 *   Windows: set FOUNDATION_TEST_SERIAL=COM1
 *   Linux:   export FOUNDATION_TEST_SERIAL=/dev/ttyS0
 *   foundation_utests --run_test=factory_serport_hardware
 */
BOOST_AUTO_TEST_CASE(factory_serport_hardware)
{
    const char* deviceName = std::getenv("FOUNDATION_TEST_SERIAL");
    if (deviceName == nullptr || deviceName[0] == '\0')
    {
        BOOST_TEST_MESSAGE("skip factory_serport_hardware: set FOUNDATION_TEST_SERIAL "
                           "(e.g. COM1 or /dev/ttyS0)");
        return;
    }

    const std::string json = std::string(R"({
      "Port": {
        "InstanceOf": "tools::os::serport::Serport",
        "DeviceName": ")") + deviceName +
                             R"("
      }
    })";
    const auto center      = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;

    auto port = create<tools::os::serport::Serport>(app, center->root()["Port"]);
    BOOST_REQUIRE(port);
    if (!port->isReady())
    {
        BOOST_TEST_MESSAGE("skip factory_serport_hardware: port not ready: " << deviceName);
        return;
    }

    BOOST_CHECK(port->isReady());
}

BOOST_AUTO_TEST_CASE(application_services_install_current_reset)
{
    reset();
    BOOST_CHECK_THROW((void)current(), std::logic_error);

    const auto center = createLocalFromString(HelloConfigJson);
    ApplicationServices services;
    services.config = center;
    install(services);
    BOOST_CHECK_EQUAL(current().root()["Hello"]["Param"].value<int>(), 7);

    reset();
    BOOST_CHECK_THROW((void)current(), std::logic_error);
}

BOOST_AUTO_TEST_CASE(create_global_objects_builds_explicit_roots)
{
    constexpr const char* json = R"({
      "GlobalObjects": [
        { "Item": "A" },
        { "Item": "B" }
      ],
      "A": {
        "InstanceOf": "tools::design::test::HelloObject",
        "Param": 1
      },
      "B": {
        "InstanceOf": "tools::design::test::HelloObject",
        "Param": 2
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    install(app);

    const auto roots = createGlobalObjects(app);
    BOOST_REQUIRE_EQUAL(roots.size(), 2u);

    auto a = obtain<test::IHello>(app, center->root(), "A");
    auto b = obtain<test::IHello>(app, center->root(), "B");
    BOOST_REQUIRE(a);
    BOOST_REQUIRE(b);
    BOOST_CHECK_EQUAL(a->param(), 1);
    BOOST_CHECK_EQUAL(b->param(), 2);
    reset();
}

BOOST_AUTO_TEST_CASE(create_global_objects_requires_list)
{
    constexpr const char* json = R"({
      "A": {
        "InstanceOf": "tools::design::test::HelloObject",
        "Param": 1
      }
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    install(app);

    BOOST_CHECK_THROW((void)createGlobalObjects(app), std::runtime_error);
    reset();
}

BOOST_AUTO_TEST_CASE(create_global_objects_fails_on_missing_item)
{
    constexpr const char* json = R"({
      "GlobalObjects": [ { "Item": "Missing" } ]
    })";

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    install(app);

    BOOST_CHECK_THROW((void)createGlobalObjects(app), std::runtime_error);
    reset();
}

BOOST_AUTO_TEST_CASE(destroy_global_objects_reverse_order_and_registry)
{
    constexpr const char* json = R"({
      "GlobalObjects": [
        { "Item": "A" },
        { "Item": "B" }
      ],
      "A": {
        "InstanceOf": "tools::design::test::DestroyProbe"
      },
      "B": {
        "InstanceOf": "tools::design::test::DestroyProbe"
      }
    })";

    test::destroyProbeOrder().clear();

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    install(app);

    auto roots = createGlobalObjects(app);
    BOOST_REQUIRE_EQUAL(roots.size(), 2u);
    BOOST_CHECK(app.instances->contains("A"));
    BOOST_CHECK(app.instances->contains("B"));

    destroyGlobalObjects(app, roots);
    BOOST_CHECK(roots.empty());
    BOOST_CHECK(!app.instances->contains("A"));
    BOOST_CHECK(!app.instances->contains("B"));
    BOOST_REQUIRE_EQUAL(test::destroyProbeOrder().size(), 2u);
    BOOST_CHECK_EQUAL(test::destroyProbeOrder()[0], "B");
    BOOST_CHECK_EQUAL(test::destroyProbeOrder()[1], "A");
    reset();
}

BOOST_AUTO_TEST_CASE(launch_global_objects_calls_ilaunchable_only)
{
    constexpr const char* json = R"({
      "GlobalObjects": [
        { "Item": "Launcher" },
        { "Item": "Plain" }
      ],
      "Launcher": {
        "InstanceOf": "tools::design::test::LaunchProbe"
      },
      "Plain": {
        "InstanceOf": "tools::design::test::HelloObject",
        "Param": 3
      }
    })";

    test::LaunchProbe::launchCount() = 0;

    const auto center = createLocalFromString(json);
    ApplicationServices app;
    app.config = center;
    install(app);

    auto roots = createGlobalObjects(app);
    BOOST_REQUIRE_EQUAL(roots.size(), 2u);
    BOOST_CHECK(roots[0].launchable != nullptr);
    BOOST_CHECK(roots[1].launchable == nullptr);
    BOOST_CHECK_EQUAL(test::LaunchProbe::launchCount(), 0);

    launchGlobalObjects(roots);
    BOOST_CHECK_EQUAL(test::LaunchProbe::launchCount(), 1);
    reset();
}
