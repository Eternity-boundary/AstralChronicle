# Tests

Place native unit and UI-test projects beneath this directory. Keep test code outside the production WinUI application project.

## Minimum regression coverage

- Service tests should cover channel queries, structured `<QueryList>` custom views,
  cancellation, error mapping, and provider metadata parsing without depending on a
  particular event-record count.
- UI tests should use stable `AutomationProperties.AutomationId` values and verify
  navigation selection, asynchronous loading states, event selection details, and
  wrapped provider metadata paths.
- Manual smoke tests should include expanding Custom Views, opening Administrative
  Events, opening an existing custom-view folder when available, and checking the
  Security access-denied recovery action on a non-elevated launch.

Run packaged app tests through Visual Studio deployment or an AppsFolder activation;
do not launch the generated executable directly.
