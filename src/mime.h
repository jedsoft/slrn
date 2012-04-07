#ifndef _SLRN_MIME_H
#define _SLRN_MIME_H

#define MIME_ERROR_WARN 0
#define MIME_ERROR_CRIT 1
#define MIME_ERROR_NET  2
typedef struct Slrn_Mime_Error_Obj
{
   struct Slrn_Mime_Error_Obj *next, *prev;
   char *err_str; /* malloc'ed */
   char *msg;
   /* true if user can't overwrite it, false else */
   int critical;
   unsigned int lineno;
} Slrn_Mime_Error_Obj;

extern int slrn_rfc1522_decode_header (char *name, char **hdrp);
extern int slrn_rfc1522_decode_string (char **s_ptr, unsigned int start_offset);
extern char *slrn_decode_base64 (char *);
extern char *slrn_decode_qp (char *);

extern Slrn_Mime_Error_Obj *slrn_mime_header_encode (char **, char *);
extern int slrn_mime_process_article (Slrn_Article_Type *);
extern void slrn_mime_init (Slrn_Mime_Type *);
extern void slrn_mime_free (Slrn_Mime_Type *);
extern Slrn_Mime_Error_Obj *slrn_add_mime_error (Slrn_Mime_Error_Obj *, char *, char *, int, int);
extern void slrn_free_mime_error(Slrn_Mime_Error_Obj *);
extern Slrn_Mime_Error_Obj *slrn_mime_concat_errors (Slrn_Mime_Error_Obj *, Slrn_Mime_Error_Obj *);
extern Slrn_Mime_Error_Obj *slrn_mime_error (char *msg, char *line, int lineno, int critical);


extern int slrn_mime_call_metamail (void);
extern Slrn_Mime_Error_Obj *slrn_mime_encode_article(Slrn_Article_Type *, char *);
extern Slrn_Mime_Error_Obj *slrn_mime_header_encode (char **, char *);


extern int Slrn_Fold_Headers;
extern int Slrn_Use_Meta_Mail;
extern char *Slrn_MetaMail_Cmd;


#endif /* _SLRN_MIME_H */
