#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "tspl_basic.h"
#include "tspl_label_func.h"
#include "tspl_parser.h"
#include "tspl_PageBuffer.h"
#include "tspl_config.h"

void tspl_parser(char *buf, uint32_t buf_len, char *file)
{
    uint32_t token_type = 0;

    if (setjmp(e_buf)) /* initialize the long jump */
        return;//TODO: add sonething here.

    BasicParser->init();
    
    if (buf != NULL) {
        if (!BasicParser->load_program_from_memory(buf, buf_len)) return;
    }
    else if (file != NULL) {
        if (!BasicParser->load_program_from_file(file)) return;
    }
    else {
        LogFatal("basic_parser entry param illeagal!");
        return;
    }
    
    BasicParser->scan_labels();

    do {
        token_type = BasicParser->get_token();
        if (token_type == VARIABLE) {
            /* 如果当前是变量 */
            BasicParser->put_back();
            BasicParser->var_assignment();
        }
        else if (token_type == STRVARIABLE) {
            /* 如果当前是字符变量 */
            BasicParser->put_back();
            BasicParser->str_assignment();
        }
        else if (token_type == DELIMITER && *BasicParser->Token == ':') {
            /* 如果是标签，跳过这行 */
            BasicParser->find_eol();
        }
        else if (token_type == STRING) {
            SERROR(26);
        }
        else {
            /* 关键字 */
            switch (BasicParser->Tok)
            {
            /* basic指令 */
            case IF_ID      : BasicParser->exec_if();         break;
            case ELSE_ID    : BasicParser->find_eol();        break;
            case FOR_ID     : BasicParser->exec_for();        break;
            case GOTO       : BasicParser->exec_goto();       break;
            case NEXT_ID    : BasicParser->exec_next();       break;
            case GOSUB_ID   : BasicParser->exec_gosub();      break;
            case RETURN_ID  : BasicParser->exec_return();     break;
            case REM_ID     : BasicParser->exec_rem();        break;
            case OPEN_ID    : BasicParser->exec_open();       break;
            case READ_ID    : BasicParser->exec_read();       break;
            case SEEK_ID    : BasicParser->exec_seek();       break;
            case END_ID     :                                 return;
            case EOP_ID     :                                 return;
            case FINISHED_ID:                                 return;
            /* 打印机指令 */
            case PRINTF_ID     : labelPrinter->exec_printf();    break;
            case SIZE_ID       : labelPrinter->exec_size();      break;
            case GAP_ID        : labelPrinter->exec_gap();       break;
            case BLINE_ID      : labelPrinter->exec_bline();     break;
            case OFFSET_ID     : labelPrinter->exec_offset();    break;
            case SPEED_ID      : labelPrinter->exec_speed();     break;
            case DENSITY_ID    : labelPrinter->exec_density();   break;
            case DIRECTION_ID  : labelPrinter->exec_direction(); break;
            case REFERENCE_ID  : labelPrinter->exec_reference(); break;
            case COUNTRY_ID    : labelPrinter->exec_country();   break;
            case CODEPAGE_ID   : labelPrinter->exec_codepage();  break;
            case CLS_ID        : labelPrinter->exec_cls();       break;
            case FEED_ID       : labelPrinter->exec_feed();      break;
            case FORMFEED_ID   : labelPrinter->exec_formfeed();  break;
            case HOME_ID       : labelPrinter->exec_home();      break;
            case PRINT_ID      : labelPrinter->exec_print();     break;
            case SOUND_ID      : labelPrinter->exec_sound();     break;
            case CUT_ID        : labelPrinter->exec_cut();       break;
            case LIMITFEED_ID  : labelPrinter->exec_limitfeed(); break;
            case SWITCHTOESC_ID: labelPrinter->exec_switch2esc(); break;
            /* 卷标内容设计指令 */
            case BAR_ID     : labelPrinter->exec_bar();       break;
            case BARCODE_ID : labelPrinter->exec_barcode();   break;
            case BITMAP_ID  : labelPrinter->exec_bitmap();    break;
            case BOX_ID     : labelPrinter->exec_box();       break;
            case ERASE_ID   : labelPrinter->exec_erase();     break;
            case DMATRIX_ID : labelPrinter->exec_dmatrix();   break;
            case MAXICODE_ID: labelPrinter->exec_maxicode();  break;
            case PDF417_ID  : labelPrinter->exec_pdf417();    break;
            case PUTPCX_ID  : labelPrinter->exec_putpcx();    break;
            case REVERSE_ID : labelPrinter->exec_reverse();   break;
            case TEXT_ID    : labelPrinter->exec_text();      break;
            /* 档案管理指令 */
            case DOWNLOAD_ID: labelPrinter->exec_download();  break;
            /* 自定义指令 */
            case DEBUG_ID       : labelPrinter->exec_debug();        break;
            case SETLINESAPCE_ID: labelPrinter->exec_setlinespace(); break;
            case SLEEP_ID       : labelPrinter->exec_sleep();        break;
            default:
                break;
            }
        }
    } while (BasicParser->Tok != FINISHED_ID);

}
