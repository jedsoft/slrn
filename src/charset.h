#ifndef _SLRN_CHARSET_H
#define _SLRN_CHARSET_H

extern void slrn_init_charset();
extern void slrn_prepare_charset();

extern int slrn_string_nonascii(char *str);

extern char *slrn_convert_substring(char *str, int offset, int len, char *to_charset, char *from_charset, int test);
extern int slrn_test_and_convert_string(char *str, char **dest, char *to_charset, char *from_charset);
extern int slrn_convert_fprintf(FILE *fp, char *to_charset, char *from_charset, const char *format, ... );

extern int slrn_convert_article(Slrn_Article_Type *a, char *to_charset, char *from_charset);
extern int slrn_test_convert_article(Slrn_Article_Type *a, char *to_charset, char *from_charset);

extern char *Slrn_Config_Charset;
extern char *Slrn_Display_Charset;
extern char *Slrn_Outgoing_Charset;
extern char *Slrn_Editor_Charset;

#endif /* _SLRN_CHARSET_H */
