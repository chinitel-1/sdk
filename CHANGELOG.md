  * `pub run` and `pub global run`

    * Faster start time for executables that don't import transformed code.

    * Binstubs for globally-activated executables are now written in the system
      encoding, rather than always in `UTF-8`. To update existing executables,
      run `pub cache repair`.
    * A transformer that tries to read a non-existent asset in another package
      will now be re-run if that asset is later created.
