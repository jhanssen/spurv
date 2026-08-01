# AGENTS.md

## Project overview

Spurv is a Vulkan-based text/code editor application written in C++. It renders its own
UI (windows, frames, text, cursors) using Vulkan, GLFW for windowing, and a
libqss/yoga based layout + styling stack (QSS-like stylesheets).

## Build system

CMake (>= 3.20), C++20 required. There are **two build modes**:

1. **Standalone executable** (the historical mode):
   ```sh
   cmake -S . -B <build-dir> -DCMAKE_BUILD_TYPE=Release
   cmake --build <build-dir>
   ```
   The `.lsdev_default` file at the repo root points at the default build dir
   (`/Users/abakken/dev/builds/spurv`) used by the lsdev tooling.

2. **Native node addon** (the current/future mode):
   ```sh
   npm install        # installs cmake-js
   npm run build      # = cmake-js build  -> build/Release/spurv.node
   npm run rebuild
   npm run clean
   ```
   `index.js` at the repo root loads `build/Release/spurv.node` and re-exports it.

Both modes share all source. The mode is selected by detecting whether cmake-js is
driving the configure (its toolchain defines `NODEJS_CMAKE_DIRECTORY`/`CMAKE_JS_INC`).
When building as a node addon, `src/main.cpp` (the `main()` entry point) is excluded and
the addon entry is `src/binding/binding.cpp` (a N-API module). When building the
standalone executable, the addon target is skipped.

Notes:

- The root `CMakeLists.txt` enables ccache automatically if `ccache` is on `PATH`.
- `ASAN` is a supported cmake option (`-DASAN=ON`) for address-sanitizer builds.
- 3rdparty libraries are built from vendored git submodules via `3rdparty/CMakeLists.txt`.
- Compiler flags for spurv's own code: `-Wall -Wextra -pedantic -Werror -Wno-comment`
  (set in `src/CMakeLists.txt`). Keep all new C++ warning-clean.
- `pkg-config` is required (`fontconfig` is found via `pkg_check_modules` in
  `src/cmake/finddeps.cmake`).

## Layout

```
CMakeLists.txt            top-level; builds 3rdparty + src
package.json / index.js   node module entry; cmake-js build scripts
3rdparty/                 vendored git submodules (see .gitmodules)
src/
  main.cpp                standalone app entry point (excluded in node mode)
  Args.h                  command-line / env parsing helpers
  app/                    App.cpp: app() entry, window + event-loop bootstrap
  binding/                N-API addon entry point (node mode)
  cmake/                  cmake helper modules (finddeps, system-link helper)
  common/                 shared utilities (Color, Geometry, Logger, Thread, ...)
  config/                 config.h.in -> generated config.h (USE_GLFW)
  document/               Document, Styleable, Layout, text classes (rope/)
  editor/                 Editor, Container, View, Frame, Cursor
  event/                  EventLoop, EventLoopUv (libuv), glfw main event loop
  render/                 Vulkan renderer, glyph atlas, pipelines, shaders
  script/                 QUICKJS SCRIPTING LAYER -- BEING REMOVED (not built)
  text/                   Font, text properties/style
  thread/                 ThreadPool, async containers
  typescript/             OLD quickjs-facing TS bundle builder -- currently not built
  window/                 Window + glfw backend
```

## Naming / link conventions

- All library code lives in `namespace spurv`.
- Header guards: `#pragma once`.
- Each subsystem has two CMake targets:
  - `<name>-object`: an OBJECT library (`add_library(spurv-editor-object OBJECT ...)`),
    aliased as `Editor::Object` etc. These hold the actual translation units.
  - `<name>-interface`: an INTERFACE library (alias `Editor`, `Render`, `Event`, ...)
    that just carries include dirs. Object libraries link what they need.
- Third-party/system libs are linked with the helper
  `target_link_libraries_system(target scope lib...)` so their include dirs are treated
  as SYSTEM (no -Werror noise from foreign headers).
- The final target (`spurv` executable or addon) links the object-library aliases plus
  the `LIBS` list of external targets.

## Scripting layer status (important)

`src/script/` (ScriptEngine, ScriptValue, ScriptClass, Promise, Builtins, fetch/process
bindings) is the old quickjs integration. It is **not built** anymore and will be
replaced by generated Node bindings. Do not invest in it.

The plan going forward:

1. Parse `.d.ts` files (declared in the cmake files) with the TypeScript compiler
   (`tsc` / TypeScript compiler API) to generate Node/N-API bindings.
2. Generated code assumes the underlying structs/classes are already implemented in
   hand-written C++ (they live in the subsystem headers above, e.g. `View`, `Document`).

See `src/typescript/src/spurv.d.ts` for the shape of the API surface that the old
engine exposed and that the new binding generator will target.

## Source control

- The repo is git with several git submodules under `3rdparty/` (see `.gitmodules`).
- Only `*.~undo-tree~` is gitignored.
- Commit messages in the history are terse and lowercase-ish ("Some work on fetch.",
  "Make release build.", "Fix."). Match that style for new commits.

## Environment

- Platform: macOS (darwin), Apple Silicon homebrew (`/opt/homebrew`).
- Node / npm available at `/opt/homebrew/bin`.
- The `nrdp-cpp-context` skill describes the local C/C++ tooling conventions; this
  project follows the same general C++ style.
