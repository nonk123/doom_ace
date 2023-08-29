(define-primitive (engine-error message)
  (use scm2str)
  ;; Don't care about the string allocation since we're crashing the program anyway.
  "{ engine_error(\"RIBBIT-USER\", \"%s\", scm2str(pop())); break; }")

(define-primitive (print message)
  (use scm2str)
  "{ char* s = scm2str(pop()); print_message(s); hack_free(s); break; }")

(define-primitive (add-tick-hook fn)
  "{ add_tick_hook((obj*)pop()); break; }")
