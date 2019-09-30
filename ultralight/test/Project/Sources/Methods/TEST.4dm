//%attributes = {}
$view:=Ultralight Create view (Screen width:C187;Screen height:C188;1)

$path:=Get 4D folder:C485(Current resources folder:K5:16)+"Takasebune"+Folder separator:K24:12+"main-ruby.xhtml"

$path:="https://miyako.github.io"
$path:="https://webdesignday.jp/samples/p3789/chapter01/"

Ultralight Load path ($view;$path)

$script:="document.body.innerHTML"

$code:=Ultralight Evaluate script ($view;$script)

$picture:=Ultralight Get snapshot ($view)

SET PICTURE TO PASTEBOARD:C521($picture)
