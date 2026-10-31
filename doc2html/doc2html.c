#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>
#include <libxml/xmlreader.h>

// 1. Native .docx parser using libzip & libxml2
void convert_docx(const char *filename) {
    int err = 0;
    zip_t *archive = zip_open(filename, ZIP_RDONLY, &err);
    if (!archive) {
        fprintf(stderr, "Error opening docx archive: %s\n", filename);
        return;
    }

    zip_file_t *xml_file = zip_fopen(archive, "word/document.xml", 0);
    if (!xml_file) {
        fprintf(stderr, "Error: word/document.xml not found in %s\n", filename);
        zip_close(archive);
        return;
    }

    struct zip_stat st;
    zip_stat_init(&st);
    zip_stat(archive, "word/document.xml", 0, &st);

    char *buffer = (char *)malloc(st.size);
    if (!buffer) {
        zip_fclose(xml_file);
        zip_close(archive);
        return;
    }

    zip_fread(xml_file, buffer, st.size);
    zip_fclose(xml_file);
    zip_close(archive);

    xmlTextReaderPtr reader = xmlReaderForMemory(buffer, (int)st.size, NULL, NULL, 0);
    if (!reader) {
        free(buffer);
        return;
    }

    printf("<!DOCTYPE html>\n<html>\n<head><meta charset='utf-8'></head>\n<body>\n");
    int in_paragraph = 0, in_text = 0;

    while (xmlTextReaderRead(reader) == 1) {
        const xmlChar *name = xmlTextReaderConstName(reader);
        if (!name) continue;
        int type = xmlTextReaderNodeType(reader);

        if (type == XML_READER_TYPE_ELEMENT && strcmp((const char *)name, "w:p") == 0) {
            if (in_paragraph) printf("</p>\n");
            printf("<p>");
            in_paragraph = 1;
        } else if (type == XML_READER_TYPE_ELEMENT && strcmp((const char *)name, "w:t") == 0) {
            in_text = 1;
        } else if (type == XML_READER_TYPE_TEXT && in_text) {
            xmlChar *value = xmlTextReaderValue(reader);
            if (value) {
                printf("%s", (char *)value);
                xmlFree(value);
            }
        } else if (type == XML_READER_TYPE_END_ELEMENT && strcmp((const char *)name, "w:t") == 0) {
            in_text = 0;
        }
    }
    if (in_paragraph) printf("</p>\n");
    printf("</body>\n</html>\n");

    xmlFreeTextReader(reader);
    xmlCleanupParser();
    free(buffer);
}

// 2. Native .rtf parser using a pure C state machine
void convert_rtf(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error opening rtf file: %s\n", filename);
        return;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *rtf = malloc(length + 1);
    if (!rtf) {
        fclose(f);
        return;
    }
    fread(rtf, 1, length, f);
    rtf[length] = '\0';
    fclose(f);

    printf("<!DOCTYPE html>\n<html>\n<head><meta charset='utf-8'></head>\n<body>\n<p>");
    int brace_depth = 0, skipping_destination = 0, in_command = 0;

    for (int i = 0; rtf[i] != '\0'; i++) {
        char c = rtf[i];
        if (c == '{') {
            brace_depth++;
            if (rtf[i+1] == '\\' && rtf[i+2] == '*') skipping_destination = brace_depth;
        } else if (c == '}') {
            if (brace_depth == skipping_destination) skipping_destination = 0;
            if (brace_depth > 0) brace_depth--;
            in_command = 0;
        } else if (skipping_destination && brace_depth >= skipping_destination) {
            continue;
        } else if (c == '\\') {
            in_command = 1;
            if (strncmp(&rtf[i], "\\par", 4) == 0) {
                printf("</p>\n<p>");
                i += 3;
            }
        } else if (in_command) {
            if (c == ' ' || c == '\n' || c == '\r') {
                in_command = 0;
            } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
                in_command = 0;
                if (brace_depth <= 1) putchar(c);
            }
        } else {
            if (brace_depth <= 1 && c != '\n' && c != '\r') {
                if (c == '<') printf("&lt;");
                else if (c == '>') printf("&gt;");
                else if (c == '&') printf("&amp;");
                else putchar(c);
            }
        }
    }
    printf("</p>\n</body>\n</html>\n");
    free(rtf);
}

// 3. Legacy .doc parser (routes to antiword)
void convert_doc(const char *filename) {
    printf("<!DOCTYPE html>\n<html>\n<head><meta charset='utf-8'></head>\n<body>\n<pre>\n");
    fflush(stdout);
    char command[512];
    snprintf(command, sizeof(command), "antiword \"%s\"", filename);
    system(command);
    printf("</pre>\n</body>\n</html>\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.docx | file.rtf | file.doc>\n", argv[0]);
        return 1;
    }

    char *ext = strrchr(argv[1], '.');
    if (!ext) {
        fprintf(stderr, "Error: File has no extension\n");
        return 1;
    }

    if (strcasecmp(ext, ".docx") == 0) {
        convert_docx(argv[1]);
    } else if (strcasecmp(ext, ".rtf") == 0) {
        convert_rtf(argv[1]);
    } else if (strcasecmp(ext, ".doc") == 0) {
        convert_doc(argv[1]);
    } else {
        fprintf(stderr, "Unsupported format: %s\n", ext);
        return 1;
    }

    return 0;
}