.\"------------
.\" Cover sheet. Memorandum type 5
.\"------------
.PH "''''"
.OH "''''"
.EH "''''"
.nr cov*mt0-ind 1.1c
.de cov@print-title
.B
.ll 9c
.cov*title
.R
.ll
.nf
.if d cov*title-charge-case \fBCharge Case \\*[cov*title-charge-case]\fP
.if d cov*title-file-case \fBFile Case \\*[cov*title-file-case]\fP
.fi
..
.\"------------
.de cov@print-date
.rj 1
.B "\\*[cov*new-date]"
.br
..
.\"------------
.if !d cov*mt-printed \{\
.	SP 1.9c
.	cov@print-title 
.	SP 1.2c
.	cov@print-date 
.	SP 3
.	pg@enable-top-trap
.	pg@enable-trap
.	ds cov*mt-printed
.\}
