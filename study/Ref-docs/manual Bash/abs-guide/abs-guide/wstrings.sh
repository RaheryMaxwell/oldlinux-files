#!/bin/bash
# wstrings.sh: "word-strings" (enhanced "strings" command)
#
#  This script filters the output of "strings" by checking it
#+ against a standard word list file.
#  This effectively eliminates all the gibberish and noise,
#+ and outputs only recognized words.

# =================================================================
#                 Standard Check for Script Argument(s)
ARGS=1
E_BADARGS=65
E_NOFILE=66

if [ $# -ne $ARGS ]
then
  echo "Usage: `basename $0` filename"
  exit $E_BADARGS
fi

if [ -f "$1" ]                        # Check if file exists.
then
    file_name=$1
else
    echo "File \"$1\" does not exist."
    exit $E_NOFILE
fi
# =================================================================


MINSTRLEN=3                           #  Minimum string length.
WORDFILE=/usr/share/dict/linux.words  #  Dictionary file.
                                      #  May specify a different
                                      #+ word list file
                                      #+ of format 1 word per line.


wlist=`strings "$1" | tr A-Z a-z | tr '[:space:]' Z | \
tr -cs '[:alpha:]' Z | tr -s '\173-\377' Z | tr Z ' '`

# Translate output of 'strings' command with multiple passes of 'tr'.
#  "tr A-Z a-z"  converts to lowercase.
#  "tr '[:space:]'"  converts whitespace characters to Z's.
#  "tr -cs '[:alpha:]' Z"  converts non-alphabetic characters to Z's,
#+ and squeezes multiple consecutive Z's.
#  "tr -s '\173-\377' Z"  converts all characters past 'z' to Z's
#+ and squeezes multiple consecutive Z's,
#+ which gets rid of all the weird characters that the previous
#+ translation failed to deal with.
#  Finally, "tr Z ' '" converts all those Z's to whitespace,
#+ which will be seen as word separators in the loop below.

#  Note the technique of feeding the output of 'tr' back to itself,
#+ but with different arguments and/or options on each pass.


for word in $wlist                    # Important:
                                      # $wlist must not be quoted here.
                                      # "$wlist" does not work.
                                      # Why?
do

  strlen=${#word}                     # String length.
  if [ "$strlen" -lt "$MINSTRLEN" ]   # Skip over short strings.
  then
    continue
  fi

  grep -Fw $word "$WORDFILE"          # Match whole words only.

done  


exit 0
