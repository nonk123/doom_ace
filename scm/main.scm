(define-primitive (error message)
  (use scm2str)
  "{ engine_error(\"RIBBIT-RUNTIME\", \"%s\", scm2str(pop())); break; }")

(error "If you see this error, the Ribbit integration works (tm)")
