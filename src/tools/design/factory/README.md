# Factory (`tools::design::factory`)

Configuration-driven instantiation: `InstanceOf` in the JSON → constructor
`(ApplicationServices&, config::Node)`.

**Foundation objectives:** simplify, standardize, make more reliable — **no type-specific or board-specific handling**
when a generic mechanism is sufficient.

**Conventions:** `ApplicationServices& app` first; the configuration node is named `node`.

**Rules:**

| Rule | Detail |
|------|--------|
| Single factory | `FOUNDATION_FACTORY_REGISTER` → `(ApplicationServices&, config::Node)` only |
| `IObject` | Every registered concrete derives from `factory::IObject` (directly or via `ObjKit`); `create` / `createShared` / `InstanceRegistry` re-type via `dynamic_cast` from that root |
| Local registry | `Objects { … }` is always **last** in the composite — instantiable definitions |
| Creation | `"Child": "first"` → `createShared<T>` (owner) / `obtain<T>` (consumer) |
| Collection `Item` creation | `"Inputs": [{ "Item": "btnUp" }, …]` → `createSharedFromItemList` / `obtainFromItemList` |
| Named creation | `createSharedNamed<T>(app, parent, "Name", props)` — creates an instance under `parent` without a file `Objects` entry |
| External reference | `"PCA9539": "IOBoard.PCA9539"` — pointed path; `obtain` through the registry if no `Objects` entry exists |
| Simple parameter | `"btnUp": "true"` — read via `node["btnUp"]`, not by the factory |

## Normal usage (production)

The JSON names the **concrete class**. The C++ code only manipulates the **interface**.

```json
"MyIn": {
  "InstanceOf": "io::in::InByPCA9539",
  "Pin": 3
}
```

```cpp
auto myIn = factory::create<io::in::IIn>(app, center->root()["MyIn"]);
```

| Role | Detail |
|------|--------|
| `InstanceOf` | Type actually constructed (registered via `FOUNDATION_FACTORY_REGISTER`) |
| `create<T>` | Type expected by the code (`T` = interface or base class) |
| `ApplicationServices` | Shared services (configuration, logging) — not the object parameters from the JSON |

No alias is required for this flow.

## `Objects` model + wiring (application wiring)

A composite declares **its configurable dependencies** through `Item` lists and `Objects`.
For an instance created in code under the current node **without** a definition in the JSON file,
use `createSharedNamed` (for example, chips imposed by a board).

```json
"IOBoard": {
  "InstanceOf": "driver::board::EsploraBoard",
  "Inputs": [
    { "Item": "btnUp" }
  ],
  "Objects": {
    "btnUp": {
      "InstanceOf": "io::in::InByPCA9539",
      "PCA9539": "IOBoard.PCA9539"
    }
  }
}
```

`Item` lists first; **`Objects` always last**.

```cpp
_mcp2221 = factory::createSharedNamed<MCP2221>(app, node, "MCP2221");
_pca9539 = factory::createSharedNamed<PCA9539>(
    app, node, "PCA9539", {{"I2CMaster", "MCP2221"}, {"Address", 116}});
factory::createSharedFromItemList<io::in::IIn>(app, node, "Inputs", _inputs);
```

| API | Role |
|-----|------|
| `create` / `createUniqueFromItemList` | Instantiates a `unique_ptr` — **not** registered |
| `createShared` / `createSharedFromItemList` | Instantiates from a configuration node and **registers** it under `config::instancePath` |
| `createSharedNamed` | Instantiates under the current node **without** a file `Objects` entry (ephemeral definition) |
| `obtain` / `obtainFromItemList` | Retrieves an already registered instance; `createIfMissing=true` allows on-demand creation |

Owner → `createShared` / lists for configuration wiring, or `createSharedNamed` for an instance
absent from the JSON; consumers → `obtain` (references like `"IOBoard.PCA9539"` even without an `Objects` entry).

### `createSharedNamed`

Creates any factory-registered object under a parent, without an `Objects` entry in the document:

```cpp
factory::createSharedNamed<T>(app, parent, "Name", props);
```

- registers under `instancePath(parent).Name` (e.g. `IOBoard.MCP2221`)
- `props` = fields read by the factory constructor (`Address`, `I2CMaster`, …), **not** persisted in the JSON
- `obtain` resolves `"IOBoard.Name"` or the short name `"Name"` relative to the parent through the registry, without an `Objects` entry

EsploraBoard example (chips imposed by the board):

```cpp
_mcp2221 = factory::createSharedNamed<MCP2221>(app, node, "MCP2221");
_pca9539 = factory::createSharedNamed<PCA9539>(
    app, node, "PCA9539", {{"I2CMaster", "MCP2221"}, {"Address", 116}});
```

### Configuration resolution

`config::follow(root, node, key)` on a string field:

- contains `.` → external reference (`IOBoard.PCA9539`); short aliases shadow `Objects`
- otherwise → `node.Objects/<name>` (local short reference)

`config::resolveNamed(root, node, name)` — binding key **or** `Objects` entry name (loop over `Item`).

`resolveReference("IOBoard.PCA9633_0")` without a top-level binding: automatic fallback to `Objects`;
if the entry is missing (chip created via `createSharedNamed`), `obtain` consults `InstanceRegistry`.

`Objects` is a **syntactic keyword** (container) in the JSON file. `instancePath` removes only
container segments — an object literally named `"Objects"` is preserved:

| Concept | Example | Usage |
|---------|---------|-------|
| Physical path `Node.path()` | `IOBoard/Objects/MCP2221` | JSON navigation (`root.at`) |
| Logical path `instancePath` | `IOBoard.MCP2221` | `InstanceRegistry` key |
| Named `Objects` instance | physical `IOBoard/Objects/Objects` → logical `IOBoard.Objects` | Instance name preserved |
| Short name `instanceName` | `MCP2221` | Logger channel (unique: `MCP2221`, `MCP2221~1`, …) |
| Local factory reference | `"MCP2221"` | `obtain` / `createShared` at the same level |
| External factory reference | `"IOBoard.MCP2221"` | from anywhere else |
| Derived instance `#bridge` | `IOBoard.btnUp#bridge` | Sidecar EventBus; not a node in the tree |

`.` separates segments originating from the JSON. `#` suffixes a **technical instance** associated with a canonical key
(never produced by `instancePath` from the physical path).

`factory::createShared` / `createSharedNamed` register under `instancePath`; `factory::obtain` reads this registry
(and only instantiates if `createIfMissing`).

### `Bridged`

Configuration shortcut: instantiate the local object **and** a sidecar EventBus, without a separate `*Bridge` object in the JSON.

```json
"btnUp": {
  "InstanceOf": "io::in::InByPCA9539",
  "PCA9539": "IOBoard.PCA9539",
  "Bridged": "io::in::InBridge"
}
```

- `InstanceOf` = local object (registered under `instancePath`, e.g. `IOBoard.btnUp`)
- `Bridged` = factory name of the bridge (`ILaunchable`)
- the bridge is registered under `derivedInstanceKey(instancePath, "bridge")` → `IOBoard.btnUp#bridge`
- the bus topic of the Ghost/Bridge uses the same canonical key **without** `#bridge` (`IOBoard.btnUp`)
- requires `app.eventBus` already wired; `launch()` of the bridge is immediate
- `destroyGlobalObjects` clears `#bridge` and then the root

Without `Bridged`, there is no sidecar. On the Remote side, `InstanceOf` points to the Ghost; no `Bridged`.

### Outside the factory

- Parameters (`"btnUp": "true"`) : direct read from the configuration node
- IO objects instantiated elsewhere (`"unknown": "root.OutUnknown"`) : global resolution, not in the
  composite `Objects`

## `Registry::addAlias` — policy

**First approach:** the configuration remains **concrete** (`InstanceOf` = real implementation).
We do not introduce abstract names into the JSON (e.g. `"InstanceOf": "io::in::IIn"`).

`addAlias(alias, target)` redirects an `InstanceOf` name to another registered name in the
registry. Resolution is chained (max depth 8) in `Registry::find`.

### Chosen case: tests and hardware-free benches

**Problem:** reuse the production JSON (or a faithful copy) in unit tests without real hardware or drivers.

**Solution:** in the test executable only, register a mock and redirect the alias
**before** the `factory::create` calls:

```cpp
Registry::instance().addAlias("io::in::InByPCA9539", "io::in::test::InMock");
```

The production JSON remains:

```json
"InstanceOf": "io::in::InByPCA9539"
```

The application code remains:

```cpp
auto myIn = factory::create<io::in::IIn>(app, node);
```

In production: no alias → real `InByPCA9539`.
In tests: alias installed (global fixture or test `main`) → `InMock`.

Benefits:

- no `#ifdef TEST` in the business logic;
- no duplication of JSON with different `InstanceOf` values;
- the mock can expose test helpers (`set(value)`, etc.).

See `tests/tools/design/factory/factory_test.cpp` — case
`factory_alias_redirects_prod_json_to_mock`.

## Registering a class

In the implementation `.cpp`:

```cpp
FOUNDATION_FACTORY_REGISTER(io::in::InByPCA9539,
                            "io::in::InByPCA9539",
                            io_in_InByPCA9539)

// Production (sample) — see EsploraBoard.cpp; linked through build/generated/FactoryLink.cpp
FOUNDATION_FACTORY_REGISTER(driver::board::EsploraBoard,
                            "driver::board::EsploraBoard",
                            driver_board_EsploraBoard)
```

Place the macro **outside** any enclosing namespace (otherwise MSVC may create an incorrect nested namespace).

`main.cpp` (sample) calls `foundationFactoryLinkAll()` and then `create<EsploraBoard>(app, root["EsploraBoard"])`.

For a static library: the application calls `foundationFactoryLinkAll()`, which references every active
`touch_*`. CMake generates `build/generated/FactoryLink.cpp` via
`foundation_generate_factory_link()` (`tools/GenerateFactoryLink.cmake`) by scanning
`FOUNDATION_FACTORY_REGISTER` in `ALL_SRCS`.

CMake: the framework target is **`foundation`** (STATIC); the sample compiles `main.cpp` and the generated
`FactoryLink.cpp`. `EXCLUDE_LINK_IDS` in `CMakeLists.txt` removes unused types from the app
(e.g. `tools_os_serport_Serport`). The `FOUNDATION_WHOLE_ARCHIVE=ON` option links the full archive if needed.

- **Type** — concrete class
- **Name** — string identical to `InstanceOf`
- **LinkId** — C identifier for `link::touch_<LinkId>()`: **fully qualified type name, with `::` replaced by `_`**
  (e.g. `driver::chip::MCP2221` → `driver_chip_MCP2221`, symbol `touch_driver_chip_MCP2221()`)

## Fichiers

| Fichier | Rôle |
|---------|------|
| `Registry.hpp` | Map nom → `Creator`, aliases |
| `Register.hpp` | `FOUNDATION_FACTORY_REGISTER`, `link::touch_*` |
| `Factory.hpp` | `createFromNode`, `create<T>` |
| `Tree.hpp` | `itemNames`, `createSharedFromItemList`, `obtainFromItemList`, `createUniqueFromItemList` |
| `Obtain.hpp` | `obtain`, `create` par nom, `createShared`, `createSharedNamed`, `Bridged` / `#bridge` |
| `config/Reference.hpp` | `follow`, `resolveNamed`, `resolveObject` |
| `ApplicationServices.hpp` | `install` / `current` / `reset`, `LogServicePtr`, `resolveNamed` |
| `LinkAll.hpp` | `foundationFactoryLinkAll()` (impl. application) |
