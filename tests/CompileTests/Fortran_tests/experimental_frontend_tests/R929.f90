CHARACTER :: cvar
INQUIRE (UNIT=10)
INQUIRE (FILE='foo.txt', BLANK=cvar)
INQUIRE (UNIT=10, BLANK=cvar)
INQUIRE (ENCODING=cvar, UNIT=10)
INQUIRE (IOLENGTH=ix) a, b, c
END PROGRAM
