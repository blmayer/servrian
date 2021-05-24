#include "head.h"
#include "defs.h"
#include <stdio.h>

int serve_head(short conn, struct request req, int status) {
    struct response res;

    puts("serving head");
    res.server = "Servrian/" VERSION;
    res.version = 1.1;
    res.status = status;
    res.date = date_line();

    /* Get the page file's size */
    if (status < 300) {
        int page_size = 0;
        res.clen = file_size(req.url);
        res.ctype = mime_type(res.path);

        if (page_size == 0) {
            /* Something went wrong, probably file was not found */
            res.status = 404;
            res.clen = 0;
        }
    } else {
        res.ctype = "text/html";
        res.clen = 0;
        res.conn = "Close";
    }

    /* Create the head */
    res.stext = status_text(res.status);
    puts("creating header");
    char head[MAX_HEADER_SIZE];
    create_res_header(res, head);
    puts(head);

    /* Send the head */
    write(conn, head, strlen(head));

    return 0;
}
