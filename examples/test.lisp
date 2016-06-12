#! ../vm_linux64

progn
(
	(declare 0 defvar '(macro (name val)
		(declare 0 name val)))

	(declare 0 defmacro '(macro (name plist def)
		(defvar name '(macro plist def))))

	(declare 0 defun '(macro (name plist def)
		(defvar name '(function plist def))))

	(defun exit (err)
		(syscall 1 err 0 0 0 0))

	(defun read (fd buf len)
		(syscall 3 fd buf len 0 0))

	(defun write (fd buf len)
		(syscall 4 fd buf len 0 0))

	(defun open (path oflags mode)
		(syscall 5 path oflags mode 0 0))

	(defun close (fd)
		(syscall 6 fd 0 0 0 0))

	(defun getpid ()
		(syscall 20 0 0 0 0 0))

	(defvar stdin 0)
	(defvar stdout 1)
	(defvar stderr 2)

	# a very simple malloc :D
	(defvar mptr 30000)
	(defun malloc (size)
		(progn 
			((resw 'mptr)
			(setw 'mptr (+ mptr size))
		)))

	(defun factorial (n)
		(eval (if (eq n 1)
			'(quote 1)
			'(* (factorial (- n 1)) n))))

	(defvar a 8)
	(write stdout "\ndeclared a: " 14)
	(printi a)

	(write stdout "factorial of a: " 17)
	(printi (factorial 8))

	(write stdout "\ncurrent pid: " 15)
	(printi (getpid))
	(write stdout "\n" 2)

	(defvar buf1 (malloc 16))
	(defvar buf2 (malloc 16))
#	(printi buf1)
#	(printi buf2)

#	(setw (+ buf1 0) 0)
#	(setw (+ buf1 4) 0)
#	(setw (+ buf1 8) 0)
#	(setw (+ buf1 12) 0)
#
#	(setw (+ buf2 0) 0)
#	(setw (+ buf2 4) 0)
#	(setw (+ buf2 8) 0)
#	(setw (+ buf2 12) 0)

#	(write stdout "Enter your name: " 18)
#	(read stdin buf1 16)

#	(write stdout "favorite color: " 17)
#	(read stdin buf2 16)

#	(write stdout "\n\nyour name is " 16)
#	(write stdout buf1 16)
#	(write stdout "your favorite color is " 23)
#	(write stdout buf2 16)

	(exit 0)
)

