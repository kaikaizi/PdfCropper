pngCrop auto-detects white margins of a scanned white-background
document and crops the four margins either according to the
specified value or using a simple detection scheme. It is published
under the 2-clause BSD licence.

To compile:
cc -lpng -O2 pngCrop.c -o pngCrop
To use:
./pngCrop /home/draco/img-0011.png

On success, the cropped file is named Oimg-0011.png (or Oxx.png
where xx.png is the original name) under the same directory.

Editing source file to suit your needs. To manually set the left /
right / upper / bottom margins in pixels to crop, uncomment line
73 and edit dims array (and comment out line 75).

The automation of margin detection (procPng()) is based on simple
thresholding of consecutive dark blotches for a row (column). The
brightness (or red component for color png) is below tol (defined
on line 51), then it is considered dark. If there are more than
"Th" consecutive dark blotches on a column (or "Tw", on a row, the
two variables), then the row (column) is
considered to have actual content and thus excluded from margin.
After cropping, the Lmargin, Rmargin, Umargin and Bmargin values on
line 50 are used to determine the remaining margins surrounding
cropped content. Play with these parameters if the output png image
is unsatisfactory.

The suffix() function generates the output file name. It is based
on *NIX file system and regards '/' symbol as directory. It
prepends character 'O' before the input png file name.

Requirement: libpng.

Automatic PDF cropping:

While there are availabe scripts to adjust margins (
http://pdfcrop.sourceforge.net/ and
http://www.ctan.org/pkg/pdfcrop), it is based on TeX formatting and
thus unable to adjust for pure image-based pdfs such as
scan-generated ones. pngCrop.c is the tiny core part of simple
image cropping. It is easy to write a shell script such as
"cropper". First thing it does is use available tool (I prefer
`mupdfextract` of mupdf package) to convert a scanned PDF file into
png images. Then use pngCrop to process the images, and use
`convert` of imagemagick package to piece processed png files into
PDF.

