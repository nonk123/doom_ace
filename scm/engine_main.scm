(define thingy 10)

(define screw-this
  (lambda ()
    (print "HELLO")
    (set! thingy (+ thingy 32))
    (engine-error (number->string thingy))))

(define another
  (lambda ()
    (engine-error "If you see this error, then Ribbit SUCKS ASS")))

(add-tick-hook 'screw-this)
;; FIXME: this line crashes everything???
;; (add-tick-hook 'another)
