#ifndef _SLRN_MIME_H
#define _SLRN_MIME_H

extern int Slrn_Use_Mime;
#define MIME_DISPLAY	1
#define MIME_SAVE       2
#define MIME_ARCHIVE	4
#define MIME_PIPE	8

# if SLRN_HAS_MIME /* rest of file in this if */
extern int slrn_set_compatible_charsets (char *);
extern int slrn_rfc1522_decode_string (char *);

extern FILE *slrn_mime_encode (FILE *);
extern void slrn_mime_header_encode (char *, unsigned int);
extern void slrn_mime_process_article (Slrn_Article_Type *);
extern void slrn_mime_add_headers (FILE *);
extern int slrn_mime_scan_file (FILE *);
extern void slrn_mime_article_init (Slrn_Article_Type *);
extern int slrn_mime_call_metamail (void);
extern int slrn_check_rfc1522 (char *);

extern char *Slrn_Mime_Display_Charset;
extern char *Slrn_Utf8_Table;
extern int Slrn_Fold_Headers;

extern int Slrn_Mime_Was_Modified;
extern int Slrn_Mime_Needs_Metamail;
extern int Slrn_Mime_Was_Parsed;
extern int Slrn_Use_Meta_Mail;
extern char *Slrn_MetaMail_Cmd;
# endif /* SLRN_HAS_MIME */
#endif /* _SLRN_MIME_H */
