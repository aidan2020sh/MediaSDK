#!/bin/bash

function convert {
    echo "Converting $Title $Name "
    sed "s#Title#$Title#g" header-template.md > header-$Name.md
    pandoc --from=markdown_github -t html5 -i header-$Name.md -o header.html --css=styles/intel_css_styles.css
    pandoc --from=markdown_github -t html5 --columns 920 -i $Name.md -o body.html --css=styles/intel_css_styles_4pdf.css
    wkhtmltopdf -B 14mm -T 14mm --title "$Title" header.html --zoom 1.5 toc --xsl-style-sheet styles/toc.xsl body.html $PDF_BODY_OPT --footer-line --footer-center "$Title" --footer-right "$APIVersion" --footer-left [page] --footer-font-size 8 --footer-font-name Verdana --footer-spacing 4 PDF/$Name.pdf

    rm header.html
    rm body.html
    rm header-$Name.md
}

APIVersion=$(grep 'API Version' header-template.md | cut -d\  -f3)

echo $APIVersion

rm -rf PDF
mkdir PDF

PDF_BODY_OPT="--zoom 1.5"
Name="mediasdk-man"
Title="SDK Developer Reference"
convert

Name="mediasdkusr-man"
Title="SDK Developer Reference Extensions for User-Defined Functions"
convert

PDF_BODY_OPT=""
Name="mediasdkjpeg-man"
Title="SDK Developer Reference for JPEG/Motion JPEG"
convert

Name="mediasdkmvc-man"
Title="SDK Developer Reference for Multi-View Video Coding"
convert

Name="mediasdkvp8-man"
Title="SDK Developer Reference for VP8"
convert

Name="mediasdkfei-man"
Title="SDK Developer Reference for FEI"
convert

echo Converting Done
