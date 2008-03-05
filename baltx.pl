#!/usr/bin/perl -w
# Create a LaTeX full of before-after pictures of states with current
# districting and solver's districting.
# Scans all two-letter directories for files resulting from manybest.pl
#
# See also baltx.pl to crate LaTeX output.
#
# Usage:
# ./baltx.pl [--full] > your.ltx
#   --full   emit complete LaTeX document, otherwise just a bunch of
#            \includegraphics statements with the found images.

$fullhtml = 0;

while ( $arg = shift @ARGV ) {
  if ( $arg eq "--full" ) {
    $fullhtml = 1;
  } else {
    print STDERR "bogus arg \"$arg\"\n";
    exit 1;
  }
}

if ( $fullhtml ) {
  print <<EOF;
\\documentclass[12pt]{article}
\\setlength{\\oddsidemargin}{0pt}
\\setlength{\\topmargin}{0pt}

\\setlength{\\headheight}{0pt}
\\setlength{\\headsep}{0pt}
\\setlength{\\topskip}{0pt}

\\setlength{\\textwidth}{\\paperwidth}
\\addtolength{\\textwidth}{-2in}
\\setlength{\\textheight}{\\paperheight}
\\addtolength{\\textheight}{-2in}

\\setlength{\\parindent}{0pt}

\\usepackage{graphicx}
\\usepackage{latexsym}
\\usepackage{times}
\\usepackage{hyperref}
\\usepackage{picinpar}

\\begin{document}
\\pagestyle{empty}

EOF
}

$count = 0;
foreach $stu ( <??> ) {
  if ( (-f "${stu}/link1/${stu}_ba_500.png") && 
       (-f "${stu}/link1/${stu}_ba.png") ) {
    print <<EOF;
\\includegraphics[width=\\textwidth]{${stu}/link1/${stu}_ba.png} \\\\
EOF
    $count++;
  }
}

if ( $fullhtml ) {
  print <<EOF;

$count states. AK, DE, MT, ND, SD, VT, WY have only 1 district.
\\end{document}
EOF
}
