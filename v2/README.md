# TODO - Discussions Pierre Etienne
- maintenir MDI par region dans l'arbre, avec dupplicat
- rechercher MDI à l'exécution en fonction du paramètre OCR
    - associer un cudaevent à un MDI
- utiliser wait sur events

# Impacts on previous applications
- users must explicitly call `xkblas_thread_init` on any thread before making any other calls to xkblas on that thread

# To improve
- Tasks descriptor is allocated in the producer thread memory... while it will be heavily accessed and modified by consumers
- Tasks are currently never deleted, as they may be referenced in the memory tree
