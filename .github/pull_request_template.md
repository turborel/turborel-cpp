<!--
This description is the series' cover letter (CONTRIBUTING.md, Pull
requests). Each commit message still stands alone, in Linux's form.
Base the series on main, keep it rebased, and on revising say what
changed in a comment, not here.
-->

## What and why

<!-- The problem and its impact, then the approach and the choices made.
Link the issue: "Closes #N". -->

## The series

<!-- One line per commit, in order: what it does. Each passes
`cmake --workflow --preset verify` on its own. -->

1.

## Testing

<!-- The tests added and what each verifies, by the heading or decision it
cites. For a change of the wire's behaviour, the live tests run. -->

- [ ] `cmake --workflow --preset verify` passes
- [ ] `cmake --workflow --preset asan-ubsan` passes
- [ ] `cmake --workflow --preset coverage` passes, its floors held or raised

## Compatibility

<!-- What changes for a program using the driver, for the headers, the
CMake package and the turborel-c it needs; "none" if nothing changes. -->

## Performance

<!-- If the change claims any: what was measured, how, with the numbers.
Otherwise "none claimed". -->

## Checklist (RULES.md §7)

- [ ] One logical change per commit, each passing `cmake --workflow --preset verify`
- [ ] The spec's heading or the decision cited in the code (R-DOC-4) and in the tests (`/// Verifies:`, R-TRACE-1)
- [ ] Normal and failure tests (R-TEST-2), a failing-first test for a fix (R-TEST-5)
- [ ] Every failure a `turborel::error`, every resource owned, no new dependency without a decision (R-ERR, R-SAFE)
- [ ] CONFORMANCE.md, RULES.md, TODO.md and `doc/` updated where they change
