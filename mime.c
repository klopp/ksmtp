/*
 * mime.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 14:15
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "mime.h"

int isUsAscii( const char * s )
{
    while( *s )
    {
        if( (unsigned char)*s > 127 ) return 0;
        s++;
    }
    return 1;
}

string mimeFileName( const char * name, const char * charset )
{
    string filename;
#ifndef __WINDOWS__
    const char * nameptr = strrchr( name, '/' );
    if( !nameptr ) nameptr = strrchr( name, '\\' );
#else
    const char * nameptr = strrchr( name, '\\' );
    if( !nameptr ) nameptr = strrchr( name, '/' );
#endif

    /*
     *  filename*=utf-8''%D0%BF%D1%80%D0%BE%D0%B1%D0%B0%2E%70%6E%67
     *   name="=?UTF-8?B?0L/RgNC+0LHQsC5wbmc=?="
     */
    if( nameptr ) name = nameptr + 1;
    if( !charset || isUsAsciiCs( charset ) || isUsAscii( name ) )
    {
        filename = sfromchar( name );
    }
    else
    {
        string b64 = base64_sencode( name );
        if( b64 )
        {
            filename = snew();
            if( !filename
                    || !sprint( filename, "=?%s?B?%s?=", charset, sstr( b64 ) ) )
            {
                sdel( b64 );
                sdel( filename );
                return NULL;
            }
            sdel( b64 );
        }
    }
    return filename;
}

char * mimeMakeBoundary( void )
{
    char * boundary = calloc( 33, 1 );
    if( !boundary ) return NULL;

    strcpy( boundary, "=-" );
    rnd_string( boundary + 2, 30 );

    return boundary;
}

int isUsAsciiCs( const char * charset )
{
    static char * usascii[] =
    { "us-ascii", "ANSI_X3.4-1968", "ANSI_X3.4-1986", "cp367", "csASCII",
            "IBM367", "iso-ir-6", "ISO646-US", "ISO_646.irv:1991", "ascii",
            "us", "us-ascii-1968", "x-ansi" };
    size_t i;

    for( i = 0; i < sizeof(usascii) / sizeof(usascii[0]); i++ )
    {
        if( !strcasecmp( usascii[i], charset ) )
        {
            return 1;
        }
    }
    return 0;
}

const char *
getMimeType( const char * filename, const char * ctype )
{
    static struct
    {
        char * ext;
        char * ctype;
    } extc[] =
    {
    { "123", "application/vnd.lotus-1-2-3" },
    { "3ds", "image/x-3ds" },
    { "669", "audio/x-mod" },
    { "a", "application/x-archive" },
    { "abw", "application/x-abiword" },
    { "ac3", "audio/ac3" },
    { "adb", "text/x-adasrc" },
    { "ads", "text/x-adasrc" },
    { "afm", "application/x-font-afm" },
    { "ag", "image/x-applix-graphics" },
    { "ai", "application/illustrator" },
    { "aif", "audio/x-aiff" },
    { "aifc", "audio/x-aiff" },
    { "aiff", "audio/x-aiff" },
    { "al", "application/x-perl" },
    { "arj", "application/x-arj" },
    { "as", "application/x-applix-spreadsheet" },
    { "asc", "text/plain" },
    { "asf", "video/x-ms-asf" },
    { "asp", "application/x-asp" },
    { "asx", "video/x-ms-asf" },
    { "au", "audio/basic" },
    { "avi", "video/x-msvideo" },
    { "aw", "application/x-applix-word" },
    { "bak", "application/x-trash" },
    { "bcpio", "application/x-bcpio" },
    { "bdf", "application/x-font-bdf" },
    { "bib", "text/x-bibtex" },
    { "bin", "application/octet-stream" },
    { "blend", "application/x-blender" },
    { "blender", "application/x-blender" },
    { "bmp", "image/bmp" },
    { "bz", "application/x-bzip" },
    { "bz2", "application/x-bzip" },
    { "c", "text/x-csrc" },
    { "c++", "text/x-c++src" },
    { "cc", "text/x-c++src" },
    { "cdf", "application/x-netcdf" },
    { "cdr", "application/vnd.corel-draw" },
    { "cer", "application/x-x509-ca-cert" },
    { "cert", "application/x-x509-ca-cert" },
    { "cgi", "application/x-cgi" },
    { "cgm", "image/cgm" },
    { "chrt", "application/x-kchart" },
    { "class", "application/x-java" },
    { "cls", "text/x-tex" },
    { "cpio", "application/x-cpio" },
    { "cpp", "text/x-c++src" },
    { "crt", "application/x-x509-ca-cert" },
    { "cs", "text/x-csharp" },
    { "csh", "application/x-shellscript" },
    { "css", "text/css" },
    { "cssl", "text/css" },
    { "csv", "text/x-comma-separated-values" },
    { "cur", "image/x-win-bitmap" },
    { "cxx", "text/x-c++src" },
    { "dat", "video/mpeg" },
    { "dbf", "application/x-dbase" },
    { "dc", "application/x-dc-rom" },
    { "dcl", "text/x-dcl" },
    { "dcm", "image/x-dcm" },
    { "deb", "application/x-deb" },
    { "der", "application/x-x509-ca-cert" },
    { "desktop", "application/x-desktop" },
    { "dia", "application/x-dia-diagram" },
    { "diff", "text/x-patch" },
    { "djv", "image/vnd.djvu" },
    { "djvu", "image/vnd.djvu" },
    { "doc", "application/vnd.ms-word" },
    { "dsl", "text/x-dsl" },
    { "dtd", "text/x-dtd" },
    { "dvi", "application/x-dvi" },
    { "dwg", "image/vnd.dwg" },
    { "dxf", "image/vnd.dxf" },
    { "egon", "application/x-egon" },
    { "el", "text/x-emacs-lisp" },
    { "eps", "image/x-eps" },
    { "epsf", "image/x-eps" },
    { "epsi", "image/x-eps" },
    { "etheme", "application/x-e-theme" },
    { "etx", "text/x-setext" },
    { "exe", "application/x-ms-dos-executable" },
    { "ez", "application/andrew-inset" },
    { "f", "text/x-fortran" },
    { "fig", "image/x-xfig" },
    { "fits", "image/x-fits" },
    { "flac", "audio/x-flac" },
    { "flc", "video/x-flic" },
    { "fli", "video/x-flic" },
    { "flw", "application/x-kivio" },
    { "fo", "text/x-xslfo" },
    { "g3", "image/fax-g3" },
    { "gb", "application/x-gameboy-rom" },
    { "gcrd", "text/x-vcard" },
    { "gen", "application/x-genesis-rom" },
    { "gg", "application/x-sms-rom" },
    { "gif", "image/gif" },
    { "glade", "application/x-glade" },
    { "gmo", "application/x-gettext-translation" },
    { "gnc", "application/x-gnucash" },
    { "gnucash", "application/x-gnucash" },
    { "gnumeric", "application/x-gnumeric" },
    { "gra", "application/x-graphite" },
    { "gsf", "application/x-font-type1" },
    { "gtar", "application/x-gtar" },
    { "gz", "application/x-gzip" },
    { "h", "text/x-chdr" },
    { "h++", "text/x-chdr" },
    { "hdf", "application/x-hdf" },
    { "hh", "text/x-c++hdr" },
    { "hp", "text/x-chdr" },
    { "hpgl", "application/vnd.hp-hpgl" },
    { "hs", "text/x-haskell" },
    { "htm", "text/html" },
    { "html", "text/html" },
    { "icb", "image/x-icb" },
    { "ico", "image/x-ico" },
    { "ics", "text/calendar" },
    { "idl", "text/x-idl" },
    { "ief", "image/ief" },
    { "iff", "image/x-iff" },
    { "ilbm", "image/x-ilbm" },
    { "iso", "application/x-cd-image" },
    { "it", "audio/x-it" },
    { "jar", "application/x-jar" },
    { "java", "text/x-java" },
    { "jng", "image/x-jng" },
    { "jp2", "image/jpeg2000" },
    { "jpe", "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "jpr", "application/x-jbuilder-project" },
    { "jpx", "application/x-jbuilder-project" },
    { "js", "application/x-javascript" },
    { "karbon", "application/x-karbon" },
    { "kdelnk", "application/x-desktop" },
    { "kfo", "application/x-kformula" },
    { "kil", "application/x-killustrator" },
    { "kon", "application/x-kontour" },
    { "kpm", "application/x-kpovmodeler" },
    { "kpr", "application/x-kpresenter" },
    { "kpt", "application/x-kpresenter" },
    { "kra", "application/x-krita" },
    { "ksp", "application/x-kspread" },
    { "kud", "application/x-kugar" },
    { "kwd", "application/x-kword" },
    { "kwt", "application/x-kword" },
    { "la", "application/x-shared-library-la" },
    { "lha", "application/x-lha" },
    { "lhs", "text/x-literate-haskell" },
    { "lhz", "application/x-lhz" },
    { "log", "text/x-log" },
    { "ltx", "text/x-tex" },
    { "lwo", "image/x-lwo" },
    { "lwob", "image/x-lwo" },
    { "lws", "image/x-lws" },
    { "lyx", "application/x-lyx" },
    { "lzh", "application/x-lha" },
    { "lzo", "application/x-lzop" },
    { "m", "text/x-objcsrc" },
    { "m15", "audio/x-mod" },
    { "m3u", "audio/x-mpegurl" },
    { "man", "application/x-troff-man" },
    { "md", "application/x-genesis-rom" },
    { "me", "text/x-troff-me" },
    { "mgp", "application/x-magicpoint" },
    { "mid", "audio/midi" },
    { "midi", "audio/midi" },
    { "mif", "application/x-mif" },
    { "mkv", "application/x-matroska" },
    { "mm", "text/x-troff-mm" },
    { "mml", "text/mathml" },
    { "mng", "video/x-mng" },
    { "moc", "text/x-moc" },
    { "mod", "audio/x-mod" },
    { "moov", "video/quicktime" },
    { "mov", "video/quicktime" },
    { "movie", "video/x-sgi-movie" },
    { "mp2", "video/mpeg" },
    { "mp3", "audio/x-mp3" },
    { "mpe", "video/mpeg" },
    { "mpeg", "video/mpeg" },
    { "mpg", "video/mpeg" },
    { "ms", "text/x-troff-ms" },
    { "msod", "image/x-msod" },
    { "msx", "application/x-msx-rom" },
    { "mtm", "audio/x-mod" },
    { "n64", "application/x-n64-rom" },
    { "nc", "application/x-netcdf" },
    { "nes", "application/x-nes-rom" },
    { "nsv", "video/x-nsv" },
    { "o", "application/x-object" },
    { "obj", "application/x-tgif" },
    { "oda", "application/oda" },
    { "ogg", "application/ogg" },
    { "old", "application/x-trash" },
    { "oleo", "application/x-oleo" },
    { "p", "text/x-pascal" },
    { "p12", "application/x-pkcs12" },
    { "p7s", "application/pkcs7-signature" },
    { "pas", "text/x-pascal" },
    { "patch", "text/x-patch" },
    { "pbm", "image/x-portable-bitmap" },
    { "pcd", "image/x-photo-cd" },
    { "pcf", "application/x-font-pcf" },
    { "pcl", "application/vnd.hp-pcl" },
    { "pdb", "application/vnd.palm" },
    { "pdf", "application/pdf" },
    { "pem", "application/x-x509-ca-cert" },
    { "perl", "application/x-perl" },
    { "pfa", "application/x-font-type1" },
    { "pfb", "application/x-font-type1" },
    { "pfx", "application/x-pkcs12" },
    { "pgm", "image/x-portable-graymap" },
    { "pgn", "application/x-chess-pgn" },
    { "pgp", "application/pgp" },
    { "php", "application/x-php" },
    { "php3", "application/x-php" },
    { "php4", "application/x-php" },
    { "pict", "image/x-pict" },
    { "pict1", "image/x-pict" },
    { "pict2", "image/x-pict" },
    { "pl", "application/x-perl" },
    { "pls", "audio/x-scpls" },
    { "pm", "application/x-perl" },
    { "png", "image/png" },
    { "pnm", "image/x-portable-anymap" },
    { "po", "text/x-gettext-translation" },
    { "pot", "text/x-gettext-translation-template" },
    { "ppm", "image/x-portable-pixmap" },
    { "pps", "application/vnd.ms-powerpoint" },
    { "ppt", "application/vnd.ms-powerpoint" },
    { "ppz", "application/vnd.ms-powerpoint" },
    { "ps", "application/postscript" },
    { "psd", "image/x-psd" },
    { "psf", "application/x-font-linux-psf" },
    { "psid", "audio/prs.sid" },
    { "pw", "application/x-pw" },
    { "py", "application/x-python" },
    { "pyc", "application/x-python-bytecode" },
    { "pyo", "application/x-python-bytecode" },
    { "qif", "application/x-qw" },
    { "qt", "video/quicktime" },
    { "qtvr", "video/quicktime" },
    { "ra", "audio/x-pn-realaudio" },
    { "ram", "audio/x-pn-realaudio" },
    { "rar", "application/x-rar" },
    { "ras", "image/x-cmu-raster" },
    { "rdf", "text/rdf" },
    { "rej", "application/x-reject" },
    { "rgb", "image/x-rgb" },
    { "rle", "image/rle" },
    { "rm", "audio/x-pn-realaudio" },
    { "roff", "application/x-troff" },
    { "rpm", "application/x-rpm" },
    { "rss", "text/rss" },
    { "rtf", "application/rtf" },
    { "rtx", "text/richtext" },
    { "s3m", "audio/x-s3m" },
    { "sam", "application/x-amipro" },
    { "scm", "text/x-scheme" },
    { "sda", "application/vnd.stardivision.draw" },
    { "sdc", "application/vnd.stardivision.calc" },
    { "sdd", "application/vnd.stardivision.impress" },
    { "sdp", "application/vnd.stardivision.impress" },
    { "sds", "application/vnd.stardivision.chart" },
    { "sdw", "application/vnd.stardivision.writer" },
    { "sgi", "image/x-sgi" },
    { "sgl", "application/vnd.stardivision.writer" },
    { "sgm", "text/sgml" },
    { "sgml", "text/sgml" },
    { "sh", "application/x-shellscript" },
    { "shar", "application/x-shar" },
    { "siag", "application/x-siag" },
    { "sid", "audio/prs.sid" },
    { "sik", "application/x-trash" },
    { "slk", "text/spreadsheet" },
    { "smd", "application/vnd.stardivision.mail" },
    { "smf", "application/vnd.stardivision.math" },
    { "smi", "application/smil" },
    { "smil", "application/smil" },
    { "sml", "application/smil" },
    { "sms", "application/x-sms-rom" },
    { "snd", "audio/basic" },
    { "so", "application/x-sharedlib" },
    { "spd", "application/x-font-speedo" },
    { "sql", "text/x-sql" },
    { "src", "application/x-wais-source" },
    { "stc", "application/vnd.sun.xml.calc.template" },
    { "std", "application/vnd.sun.xml.draw.template" },
    { "sti", "application/vnd.sun.xml.impress.template" },
    { "stm", "audio/x-stm" },
    { "stw", "application/vnd.sun.xml.writer.template" },
    { "sty", "text/x-tex" },
    { "sun", "image/x-sun-raster" },
    { "sv4cpio", "application/x-sv4cpio" },
    { "sv4crc", "application/x-sv4crc" },
    { "svg", "image/svg+xml" },
    { "swf", "application/x-shockwave-flash" },
    { "sxc", "application/vnd.sun.xml.calc" },
    { "sxd", "application/vnd.sun.xml.draw" },
    { "sxg", "application/vnd.sun.xml.writer.global" },
    { "sxi", "application/vnd.sun.xml.impress" },
    { "sxm", "application/vnd.sun.xml.math" },
    { "sxw", "application/vnd.sun.xml.writer" },
    { "sylk", "text/spreadsheet" },
    { "t", "application/x-troff" },
    { "tar", "application/x-tar" },
    { "tcl", "text/x-tcl" },
    { "tcpalette", "application/x-terminal-color-palette" },
    { "tex", "text/x-tex" },
    { "texi", "text/x-texinfo" },
    { "texinfo", "text/x-texinfo" },
    { "tga", "image/x-tga" },
    { "tgz", "application/x-compressed-tar" },
    { "theme", "application/x-theme" },
    { "tif", "image/tiff" },
    { "tiff", "image/tiff" },
    { "tk", "text/x-tcl" },
    { "torrent", "application/x-bittorrent" },
    { "tr", "application/x-troff" },
    { "ts", "application/x-linguist" },
    { "tsv", "text/tab-separated-values" },
    { "ttf", "application/x-font-ttf" },
    { "txt", "text/plain" },
    { "tzo", "application/x-tzo" },
    { "ui", "application/x-designer" },
    { "uil", "text/x-uil" },
    { "ult", "audio/x-mod" },
    { "uni", "audio/x-mod" },
    { "uri", "text/x-uri" },
    { "url", "text/x-uri" },
    { "ustar", "application/x-ustar" },
    { "vcf", "text/x-vcalendar" },
    { "vcs", "text/x-vcalendar" },
    { "vct", "text/x-vcard" },
    { "vob", "video/mpeg" },
    { "voc", "audio/x-voc" },
    { "vor", "application/vnd.stardivision.writer" },
    { "vpp", "application/x-extension-vpp" },
    { "wav", "audio/x-wav" },
    { "wb1", "application/x-quattropro" },
    { "wb2", "application/x-quattropro" },
    { "wb3", "application/x-quattropro" },
    { "wk1", "application/vnd.lotus-1-2-3" },
    { "wk3", "application/vnd.lotus-1-2-3" },
    { "wk4", "application/vnd.lotus-1-2-3" },
    { "wks", "application/vnd.lotus-1-2-3" },
    { "wmf", "image/x-wmf" },
    { "wml", "text/vnd.wap.wml" },
    { "wmv", "video/x-ms-wmv" },
    { "wpd", "application/vnd.wordperfect" },
    { "wpg", "application/x-wpg" },
    { "wri", "application/x-mswrite" },
    { "wrl", "model/vrml" },
    { "xac", "application/x-gnucash" },
    { "xbel", "application/x-xbel" },
    { "xbm", "image/x-xbitmap" },
    { "xcf", "image/x-xcf" },
    { "xhtml", "application/xhtml+xml" },
    { "xi", "audio/x-xi" },
    { "xla", "application/vnd.ms-excel" },
    { "xlc", "application/vnd.ms-excel" },
    { "xld", "application/vnd.ms-excel" },
    { "xll", "application/vnd.ms-excel" },
    { "xlm", "application/vnd.ms-excel" },
    { "xls", "application/vnd.ms-excel" },
    { "xlt", "application/vnd.ms-excel" },
    { "xlw", "application/vnd.ms-excel" },
    { "xm", "audio/x-xm" },
    { "xmi", "text/x-xmi" },
    { "xml", "text/xml" },
    { "xpi", "application/x-xpinstall" },
    { "xpm", "image/x-xpixmap" },
    { "xsl", "text/x-xslt" },
    { "xslfo", "text/x-xslfo" },
    { "xslt", "text/x-xslt" },
    { "xul", "application/vnd.mozilla.xul+xml" },
    { "xwd", "image/x-xwindowdump" },
    { "z", "application/x-compress" },
    { "zabw", "application/x-abiword" },
    { "zip", "application/zip" },
    { "zoo", "application/x-zoo" } };

    char * ext;
    char * mtype = "application/unknown";
    size_t i;
    char c;

    if( ctype ) return ctype;

    ext = strrchr( filename, '.' );
    if( !ext ) return mtype;

    ext++;
    if( !*ext ) return mtype;
    c = tolower( *ext );
    ext++;

    for( i = 0; i < sizeof(extc) / sizeof(extc[0]); i++ )
    {
        if( c == tolower( extc[i].ext[0] )
                && !strcasecmp( ext, extc[i].ext + 1 ) )
        {
            mtype = extc[i].ctype;
            break;
        }
    }

    return mtype;
}
