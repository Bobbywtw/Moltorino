# Windows development build validation

Work branch: `moltorino-build-optimization-01`, based on
`liquid-glass-integration-01` at `d4b4da3c39d717f6dac56d6921eca1657d78162a`.
The application UI, provider/account code, Qt Quick materials, shaders, version,
normal settings and production feature flags are preserved. The only application
source edit expands the existing, opt-in CI UI review; normal launches do not
execute it.

## Fast checks and full builds

| Changes | Required validation |
| --- | --- |
| Isolated `tools/glass-lab` QML, shader resources, lab executable or lab tests | Fast Windows workflow; no Conan or Moltorino C++ rebuild |
| Application C++/headers, main CMake, `GlassIntegrationPreview.qml`, native integration, dependency profiles, deployment or release configuration | Full Windows workflow, including the fast GPU checks |
| Production package delivery | Successful full workflow and `Moltorino-Windows` artifact |

The fast path parses every QML file without rewriting it, instantiates both real
scenes and reusable components, compiles the actual `.qsb` vertex and fragment
shaders, and runs them through Qt Quick's D3D11 RHI backend. Tests verify real
composer pixel changes between material ON/OFF, keep the resized composer within
the viewport, restore the original dimensions, and save visual regression
captures and measurements. It rejects a software scene graph that cannot execute
the shader. No visual/shader parameters were changed for these tests.

For the native Qt Widgets composer, fast checks reuse a successful production
package from the same branch **only** if its native source fingerprint, source
commit provenance, Qt version, Nightly state and LTO mode match. The actual
executable runs single-line, multiline, narrow-window and restored-window tests
with unchanged synthetic settings fixtures and an isolated runtime PATH.
The updated lab QML/QSB resources are tested by the freshly built GPU executable;
the reused application does not claim to contain updated embedded resources.
If native inputs differ or no verified full package exists, the job explicitly
reports that the composer requires a full build. It never substitutes a stale
executable for changed C++ validation.

The full workflow retains Windows 2022/VS2022, Qt 6.9.3, Ninja, the existing
`RelWithDebInfo` production configuration, LTO, stable M15.5.2 and Nightly off.
It builds a fresh CMake tree, deploys Qt/Quick imports and VC runtimes, verifies
the extracted ZIP, launches the actual app and FFmpeg/FFprobe, and checks settings
preservation. A manual `development_build` input disables LTO for disposable
application checks and publishes `Moltorino-Development-Windows`; this is never
selected as a production package or reused by the fast production-package check.
No timing claim is made for that optional mode until it is measured.

## Cache findings and changes

Builds #16 and #17 both missed Conan and sccache completely. #17 compiled 13
Conan packages from source and had **0/649** compiler hits. These were first
builds on new development branches, while the earlier successful cache belonged
to a sibling branch. [GitHub's cache scope](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching)
does not permit a branch to restore arbitrary sibling-branch caches. An unchanged
key does not make those caches globally readable.

The previous Conan key also appended every run ID, forcing another approximately
388 MB cache upload even when dependencies were unchanged. The compiler action
saved a timestamped approximately 1.4 GB disk cache after every run. Repeated full
copies consume repository cache space and increase eviction pressure.

The optimized setup:

- Uses a Conan cache keyed by actual MSVC file version, Windows SDK, detected
  compiler profile, Conan 2.28.1, Qt 6.9.3, the dependency recipe and the checked-in
  Windows dependency profile. Exact hits are not uploaded again. Intentional
  dependency/profile/compiler/Qt changes invalidate it. The resolved Conan lock
  is included in build metadata for inspection and reproduction.
- Preserves the working libwebm conformance, libvpx/YASM `UseEnv`, and
  FFmpeg/dav1d/pkg-config configuration.
- Uses the OpenSSL recipe's supported `user.openssl:windows_use_jom` setting
  with pinned, build-only **JOM 1.1.4** and four jobs. #17 used serial `nmake` for
  roughly ten minutes of OpenSSL work. Library versions, cryptographic features,
  compiler/runtime settings and application requirements are unchanged.
- Retains sccache with an explicit persistent directory and data/statistics
  assertions. The setup action only installs/configures the tool, using its
  documented `restore: false` and `save: false` inputs; one explicit cache system
  owns transfers. The server is stopped before restoring files so its first
  build request indexes the restored disk cache.
- Saves a complete compiler base once per compatibility fingerprint/LTO mode.
  Later runs restore that base plus a cumulative overlay containing only object
  entries absent from the base. Only the overlay needs uploading for changed
  commits. These remain normal sccache entries validated by compiler/input hashes.
- Shares the same Qt cache configuration between fast and full jobs.
- Serializes full builds per branch without cancelling an active build, allowing
  its dependency/compiler caches to finish before the next full build starts.
  Isolated fast checks still run independently.

No CMake build directories are cached, and no prior Windows runner disk is
assumed to survive. The first build on a new branch is still cold under GitHub's
access rules. A trusted default-branch cache publisher could share a common
baseline in the future; this change does not modify or merge into the default or
stable branch.

JOM is Qt's open-source GPL build tool, used to run the existing OpenSSL makefile
in parallel. It is not linked into Moltorino and is not a runtime dependency.
The [Conan recipe](https://github.com/conan-io/conan-center-index/tree/master/recipes/jom/all)
checks the official Qt 1.1.4 archive against SHA256
`d533c1ef49214229681e90196ed2094691e8c4a0a0bef0b2c901debcb562682b`.
No downloaded effect libraries, telemetry, administrator steps or authentication
changes are introduced.

## Measurements

Times below are job wall times, including setup, transfers, verification and
post-job steps, excluding queue delay. The legacy “Build Moltorino” step includes
configuration, C++ compilation and linking. The warm workflow measures the end-to-end app build command and reads native
object/link command spans from Ninja without splitting its dependency graph.
Object and link spans can overlap; compare the complete app-build command plus
configuration against the legacy combined step. The initial cold experiment used
a separate object-library command and remaining-target/link command; its sum is
reported with that distinction.

| Observed run | Cache state | Windows job total | Conan install | Configure/compile/link | Packaging |
| --- | --- | ---: | ---: | ---: | ---: |
| [Original #17](https://github.com/Bobbywtw/Moltorino/actions/runs/42981519523) | Cold; 0/649 compiler hits | 44m 10s | 19m 43s | 20m 49s | 4s |
| [Cold full #18](https://github.com/Bobbywtw/Moltorino/actions/runs/43076969357) | Cold; 0/649 compiler hits | 46m 40s, including 75s experiment | 15m 02s | 26m 05s | 7s |
| [Fast #7](https://github.com/Bobbywtw/Moltorino/actions/runs/43078041469) | Qt restored; no Conan/app compile | 1m 31s | None | Lab/QSB compilation 7s | Lab deployment 2s |

The first fast run had no previous verified full package on this new branch and
reported native-composer deferral explicitly. Later matching-package reuse must
be measured separately. The cold full run did not improve the overall total versus #17. Its Conan step
was about 24% shorter, while native compilation/linking took longer. The separate
cold-stage command measurements were 21.068s configuration, 1263.466s core C++,
and 275.772s remaining targets/linking. These observations include runner
variation and are not a controlled per-change performance attribution. The warm
full result is recorded after its workflow completes.

Build metadata contains stage JSON, raw sccache statistics, source provenance and
the Conan lock. The optional `benchmark_parallelism` input (also enabled by an
explicit `[benchmark-build]` commit marker) recompiles a representative group of
real heavy translation units at four and six jobs, records elapsed time, minimum
free physical memory and peak compiler working set, and bypasses cache hits for
that experiment. It runs after the verified application has been built. Its extra
time must be distinguished from the normal production path.

Standard public Windows runners currently have
[four CPU cores and 16 GB memory](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
The eight-heavy-object experiment bypassed sccache hits and measured:

| Jobs | Seconds | Minimum free RAM | Peak compiler working set | Exit |
| ---: | ---: | ---: | ---: | ---: |
| 4 | 38.560 | 11,421 MB | 2,349 MB | 0 |
| 6 | 35.859 | 11,522 MB | 2,269 MB | 0 |

Six jobs were approximately 7% faster in this one representative sample, with
ample memory headroom. The warm full workflow tests six application jobs; Conan
and JOM retain four. This is not a complete cold-application comparison or a
universal speed guarantee.
