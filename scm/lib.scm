(define-primitive (engine-error message)
  (use scm2str)
  "{ engine_error(\"RIBBIT\", \"%s\", scm2str(pop())); break; }")
