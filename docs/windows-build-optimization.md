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
| [Warm full #19, attempt 1](https://github.com/Bobbywtw/Moltorino/actions/runs/43084679039) | Conan/base compiler cache restored; 644/649 hits | 14m 27s | 1s after 25s restore | 10m 03s | 7s |
| [Warm full #19, attempt 2](https://github.com/Bobbywtw/Moltorino/actions/runs/43084679039) | Conan, compiler base and changed-entry cache restored; 645/649 hits | 12m 30s | 2s after 23s restore | 8m 14s | 6s |
| [Fast #7](https://github.com/Bobbywtw/Moltorino/actions/runs/43078041469) | Qt restored; no Conan/app compile | 1m 31s | None | Lab/QSB compilation 7s | Lab deployment 2s |
| [Fast #8](https://github.com/Bobbywtw/Moltorino/actions/runs/43084679086) | Qt restored; native composer reused from verified #18 | 1m 41s | None | Lab/QSB compilation 6s; no app compile | Lab deployment 2s |

The first fast run had no previous verified full package on this new branch and
reported native-composer deferral explicitly. Fast #8 then selected successful
full #18, verified the matching native fingerprint, and ran the actual application
composer checks in ten seconds without recompiling Moltorino. All three settings
fixtures preserved their contents, multiline input grew, narrow-window resizing
kept the text, and restoring the window restored the original composer height. The cold full run did not improve the overall total versus #17. Its Conan step
was about 24% shorter, while native compilation/linking took longer. The separate
cold-stage command measurements were 21.068s configuration, 1263.466s core C++,
and 275.772s remaining targets/linking. These observations include runner
variation and are not a controlled per-change performance attribution. The warm full job completed in 14m 27s with production LTO enabled. Its actual
app build command took 555.687s; native-object command span was 229.625s and the
final executable link took 325.922s. Configuration took 39.660s, and the existing
real version-object preflight accounts for the remainder of the 603s combined
build step. Packaging measured 6.955s. These spans are not added twice.

On the warm run, Conan restored in 25s, installed in 0.909s and skipped its cache
upload. sccache restored the base in 12s and achieved **644/649 hits (99.23%)**,
with zero non-cacheable calls. Only five changed entries, **4,824,581 bytes**,
needed uploading, in two seconds; the 1,398,854,997-byte base was not uploaded
again. The unchanged second attempt then restored both the base and the exact
changed-entry cache successfully. The real version-object preflight was a cache
hit, and the final build recorded **645/649 hits (99.38%)**, four misses and zero
non-cacheable calls. Exact Conan/base/overlay hits required no cache uploads.
Base restore took 17s, overlay restore 1s, applying it 1s, and Conan restore 23s.
The actual Conan install command took 0.698s (the complete install/cleanup step
rounded to 2s).

That final Windows job completed in **12m 30s**. Its app build command took
456.267s, native-object command span 184.734s, and executable link 271.407s.
Configuration took 33.685s; the complete configuration/preflight/build step took
494s. Packaging measured 5.878s. This validates changed-entry cache consumption
as well as the earlier successful save. These are observed runs with explicit
cache states, not a claim that every cold build will finish in twelve minutes.

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
ample memory headroom. Six application jobs passed the warm full workflow; Conan
and JOM retain four. This is not a complete cold-application comparison or a
universal speed guarantee.


## Final package and scope verification

The final full build uses commit `ade7ee7ce0ca1c3378a9197525bb9abf11b06435`.
The report-only commit that follows it does not alter application inputs.
`Moltorino-Windows` is available under Artifacts on successful full run #19.
GitHub sign-in is required for Actions artifact downloads.

The final ZIP has 163 entries and contains `Moltorino7.exe`, the native glass
plugin, Qt Quick/QML imports, Qt/SSL/VC runtimes and the existing media/7zip tools.
There is no modes/portable marker, JOM, CI helper executable or PDB. PE import
inspection verified all 21 Qt/OpenSSL/VC DLL names in the package (110 distinct
imports including OS libraries). The final file manifest matches the inspected
first warm package.

The real extracted executable reported exactly `Moltorino M15.5.2`, Qt 6.9.3,
Nightly false, portable false, a clean source identity, and normal
`%APPDATA%\Chatterino2\Settings`. It launched successfully with an isolated
system-only PATH, as did FFmpeg and FFprobe 7.1.5. All three actual UI profiles
preserved their settings and input text through multiline/narrow/restored tests.
The source changes do not modify providers, accounts, moderation, badges, paints,
emotes, commands, plugins, application versioning or normal settings logic.
No stable/default-branch merge or larger Liquid Glass production integration was
performed.

Final inner ZIP SHA256:
`f390aad73e884dbf6d32948293720769d7a7f9dcb71356dd533d88bf83a8c6fc`.
