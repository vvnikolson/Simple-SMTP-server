//
// Created by vmald on 4/22/19.
//

// reply codes from here
// https://www.greenend.org.uk/rjk/tech/smtpreplies.html

#ifndef SOC_REPLIES_H
#define SOC_REPLIES_H

typedef struct {
    int code;
    size_t len;
    char *str;
} reply;
#define STR(c, s) {c, sizeof(s) - 1, s }

reply reply_codes[] = {
/* 0 */        STR(200, "200 Mail OK\r\n"),
/* 1 */        STR(211, "211 System status, or system help reply\r\n"),
/* 2 */        STR(214, "214 Help message\r\n"),
/* 3 */        STR(220, "220 SMTP Service ready\r\n"),
/* 4 */        STR(221, "221 Service closing transmission channel\r\n"),
/* 5 */        STR(250, "250 Requested mail action okay, completed\r\n"),
/* 6 */        STR(354, "354 Start mail input; end with <CRLF>.<CRLF>\r\n"),
/* 7 */        STR(421, "421 Service not available, closing transmission channel\r\n"),
/* 8 */        STR(451, "451 Requested action aborted: error in processing\r\n"),
/* 9 */        STR(500, "500 Syntax error, command unrecognised\r\n"),
/* 10 */       STR(501, "501 Syntax error in parameters or arguments\r\n"),
/* 11 */       STR(502, "502 Command not implemented\r\n"),
/* 12 */       STR(503, "503 Bad sequence of commands\r\n"),
/* 13 */       STR(504, "504 Command parameter not implemented\r\n"),
/* 14 */       STR(551, "551 User not local\r\n")};

#endif //SOC_REPLIES_H
