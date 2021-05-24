#include "head.h"
#include "defs.h"

int serve_head(short conn, struct request req, int status) {
    struct response res;

    res.server = "Servrian/" VERSION;
    res.version = 1.1;
    res.status = status;

    /* Get the page file's size */
    if (status < 300) {
        int page_size = 0;
        res.clen = file_size(req.url);

        if (page_size == 0) {
            /* Something went wrong, probably file was not found */
            res.status = 404;
            res.clen = 0;
        }
    } else {
        res.ctype = "text/html";
        res.conn = "Close";
    }

    /* Create the head */
    char head[MAX_HEADER_SIZE];
    create_res_header(res, head);

    /* Send the head */
    write(conn, head, strlen(head));

    return 0;
}
