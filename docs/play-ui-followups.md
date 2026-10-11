# Play UI follow-ups (deferred from the 2026-10 review fix run)

The user moved play-UI work out of the fix run on 2026-10-11: it needs play testing and is cheap to fix
later. Nothing below is an engine defect. None of it was exercised in a browser.

- **God mode default with two human seats.** `main.cpp` turns god mode on whenever a seat is human, and god
  mode reveals every hand and private log line to every client. Seat-bound sessions (per-seat frames and log
  lines) therefore do nothing unless `--god-mode off` is given. Proposed: default to off when both seats are
  human and `--hot-seat` is not given. (plans/09 9.QQ2)
- **Two humans on one screen now need `--hot-seat`.** Without it a page opened with no `?seat=P1|P2` in its
  URL is a spectator and cannot answer.
- **Never run:** the page's JavaScript for `?seat=`, the "connected as P1" label, how the page looks to a seat
  that is not being asked (it receives an empty option list), the `--hot-seat` and `seat_bound` wiring in
  `main.cpp` with two human seats, the `ASK_PROMPT:` title reaching the browser.
- **Not tested:** a seat being released when its client disconnects.
- **HTML replay decision panel** still prints `Decision #N [<phase>]`; the trace header now carries the
  prompt's own text (plans/09 9.PP1), the panel does not.
