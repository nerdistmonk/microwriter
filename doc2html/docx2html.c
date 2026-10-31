#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>
#include <libxml/xmlreader.h>

// Process the XML content of word/document.xml to generate HTML
void parse_document_xml(const char *xml_data, size_t len) {
    xmlTextReaderPtr reader = xmlReaderForMemory(xml_data, (int)len, NULL, NULL, 0);
    if (reader == NULL) {
        fprintf(stderr, "Failed to parse document.xml\n");
        return;
    }

    int in_paragraph = 0;
    int in_text = 0;

    printf("<!DOCTYPE html>\n<html>\n<head><meta charset='utf-8'></head>\n<body>\n");

    while (xmlTextReaderRead(reader) == 1) {
        const xmlChar *name = xmlTextReaderConstName(reader);
        if (name == NULL) continue;

        int type = xmlTextReaderNodeType(reader);

        // Check for Paragraph start <w:p>
        if (type == XML_READER_TYPE_ELEMENT && strcmp((const char *)name, "w:p") == 0) {
            if (in_paragraph) printf("</p>\n");
            printf("<p>");
            in_paragraph = 1;
        }
        // Check for Text node container <w:t>
        else if (type == XML_READER_TYPE_ELEMENT && strcmp((const char *)name, "w:t") == 0) {
            in_text = 1;
        }
        // Capture text content inside <w:t>...</w:t>
        else if (type == XML_READER_TYPE_TEXT && in_text) {
            xmlChar *value = xmlTextReaderValue(reader);
            if (value) {
                printf("%s", (char *)value);
                xmlFree(value);
            }
        }
        // End of <w:t>
        else if (type == XML_READER_TYPE_END_ELEMENT && strcmp((const char *)name, "w:t") == 0) {
            in_text = 0;
        }
    }

    if (in_paragraph) printf("</p>\n");
    printf("</body>\n</html>\n");

    xmlFreeTextReader(reader);
    xmlCleanupParser();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.docx>\n", argv[0]);
        return 1;
    }

    int err = 0;
    zip_t *archive = zip_open(argv[1], ZIP_RDONLY, &err);
    if (!archive) {
        fprintf(stderr, "Error opening zip archive: %s\n", argv[1]);
        return 1;
    }

    // Locate word/document.xml inside the docx archive
    zip_file_t *xml_file = zip_fopen(archive, "word/document.xml", 0);
    if (!xml_file) {
        fprintf(stderr, "Error: word/document.xml not found in docx (not a valid docx file?)\n");
        zip_close(archive);
        return 1;
    }

    // Get file stats to find size
    struct zip_stat st;
    zip_stat_init(&st);
    zip_stat(archive, "word/document.xml", 0, &st);

    char *buffer = (char *)malloc(st.size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        zip_fclose(xml_file);
        zip_close(archive);
        return 1;
    }

    zip_fread(xml_file, buffer, st.size);
    zip_fclose(xml_file);
    zip_close(archive);

    // Parse and print HTML output
    parse_document_xml(buffer, st.size);

    free(buffer);
    return 0;
}