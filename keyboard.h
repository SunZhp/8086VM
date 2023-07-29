#ifndef VM_KEYBOARD_H
#define VM_KEYBOARD_H

/*************键盘上各键ascii码*********************************/
//

/************键盘上各键的扫描码以及组合键的扫描码***************/
#define KEY_S_L1 0x4F  /*小写键盘上的键*/
#define KEY_S_L2 0x50
#define KEY_S_L3 0x51
#define KEY_S_L4 0x4B
#define KEY_S_L6 0x4D
#define KEY_S_L7 0x47
#define KEY_S_L8 0x48
#define KEY_S_L9 0x49
#define KEY_S_ADD 0x2B
#define KEY_S_SUB 0x2D
#define KEY_S_LEFT 75      /*左箭头*/
#define KEY_S_RIGHT 77     /*右箭头*/
#define KEY_S_UP 72        /*上箭头*/
#define KEY_S_DOWN 80      /*下箭头*/
#define KEY_S_F1 59
#define KEY_S_F2 60
#define KEY_S_F3 61
#define KEY_S_F4 62
#define KEY_S_F5 63
#define KEY_S_F6 64
#define KEY_S_F7 65
#define KEY_S_F8 66
#define KEY_S_F9 67
#define KEY_S_F10 68
#define KEY_S_INSERT 82
#define KEY_S_HOME 71
#define KEY_S_PAGEUP 73
#define KEY_S_PAGEDOWN 81
#define KEY_S_DEL 83
#define KEY_S_END 79
#define KEY_S_DASH 12 /* _- */
#define KEY_S_EQUAL 13 /* += */
#define KEY_S_LBRACKET 26 /* {[ */
#define KEY_S_RBRACKET 27 /* }] */
#define KEY_S_SEMICOLON 39 /* :; */
#define KEY_S_RQUOTE 40 /* ' */
#define KEY_S_LQUOTE 41 /* ~` */
#define KEY_S_PERIOD 52 /* >. */
#define KEY_S_COMMA 51 /* <, */
#define KEY_S_SLASH 53 /* ?/ */
#define KEY_S_BACKSLASH 43 /* |" */
#define KEY_S_ENTER 28         /*回车键*/
#define KEY_S_BACKSPACE 14     /*退格键*/
#define KEY_S_SPACE 57         /*空格键*/
#define KEY_S_TAB 15
#define KEY_S_ESC 1
#define KEY_S_Q 16
#define KEY_S_W 17
#define KEY_S_E 18
#define KEY_S_R 19
#define KEY_S_T 20
#define KEY_S_Y 21
#define KEY_S_U 22
#define KEY_S_I 23
#define KEY_S_O 24
#define KEY_S_P 25
#define KEY_S_A 30
#define KEY_S_S 31
#define KEY_S_D 32
#define KEY_S_F 33
#define KEY_S_G 34
#define KEY_S_H 35
#define KEY_S_J 36
#define KEY_S_K 37
#define KEY_S_L 38
#define KEY_S_Z 44
#define KEY_S_X 45
#define KEY_S_C 46
#define KEY_S_V 47
#define KEY_S_B 48
#define KEY_S_N 49
#define KEY_S_M 50
#define KEY_S_1 2
#define KEY_S_2 3
#define KEY_S_3 4
#define KEY_S_4 5
#define KEY_S_5 6
#define KEY_S_6 7
#define KEY_S_7 8
#define KEY_S_8 9
#define KEY_S_9 10
#define KEY_S_0 11

/*++++++++++++++++++++++++CTR+各键扫描码++++++++++++++++++++++++*/
#define KEY_CTRL_F1 0x5E
#define KEY_CTRL_F2 0x5F
#define KEY_CTRL_F3 0x60
#define KEY_CTRL_F4 0x61
#define KEY_CTRL_F5 0x62
#define KEY_CTRL_F6 0x63
#define KEY_CTRL_F7 0x64
#define KEY_CTRL_F8 0x65
#define KEY_CTRL_F9 0x66
#define KEY_CTRL_F10 0x67
#define KEY_CTRL_2 0x03
#define KEY_CTRL_6 0x1E
#define KEY_CTRL_Q 0x11
#define KEY_CTRL_W 0x17
#define KEY_CTRL_E 0x05
#define KEY_CTRL_R 0x12
#define KEY_CTRL_T 0x14
#define KEY_CTRL_Y 0x19
#define KEY_CTRL_U 0x15
#define KEY_CTRL_I 0x09
#define KEY_CTRL_O 0x0F
#define KEY_CTRL_P 0x10
#define KEY_CTRL_LBRACKET 0x1B   /* {[ */
#define KEY_CTRL_RBRACKET 0x1D   /* }] */
#define KEY_CTRL_A 0x01
#define KEY_CTRL_S 0x13
#define KEY_CTRL_D 0x04
#define KEY_CTRL_F 0x06
#define KEY_CTRL_G 0x07
#define KEY_CTRL_H 0x08
#define KEY_CTRL_J 0x0A
#define KEY_CTRL_K 0x0B
#define KEY_CTRL_L 0x0C
#define KEY_CTRL_Z 0x1A
#define KEY_CTRL_X 0x18
#define KEY_CTRL_C 0x03
#define KEY_CTRL_V 0x16
#define KEY_CTRL_B 0x02
#define KEY_CTRL_N 0x0E
#define KEY_CTRL_M 0x0D
#define KEY_CTRL_SPACE 0x20
#define KEY_CTRL_BACKSPACE 0x7F
#define KEY_CTRL_ENTER 0x0A
#define KEY_CTRL_BACKSLASH 0x1C  /* |" */
#define KEY_CTRL_L1 0x75  /*小写键盘上的键*/
#define KEY_CTRL_L3 0x76
#define KEY_CTRL_L4 0x73
#define KEY_CTRL_L6 0x74
#define KEY_CTRL_L7 0x77
#define KEY_CTRL_L9 0x84

/*++++++++++++++++++++++++SHIFT+各键扫描码++++++++++++++++++++++++*/
#define KEY_SHIFT_LQUOTE 0x7E   /* ~` */
#define KEY_SHIFT_1 0x21
#define KEY_SHIFT_2 0x40
#define KEY_SHIFT_3 0x23
#define KEY_SHIFT_4 0x24
#define KEY_SHIFT_5 0x25
#define KEY_SHIFT_6 0x5E
#define KEY_SHIFT_7 0x26
#define KEY_SHIFT_8 0x2A
#define KEY_SHIFT_9 0x28
#define KEY_SHIFT_0 0x29
#define KEY_SHIFT_DASH 0x5F   /* _- */
#define KEY_SHIFT_EQUAL 0x2B  /* += */
#define KEY_SHIFT_BACKSPACE 0x08
#define KEY_SHIFT_Q 0x51
#define KEY_SHIFT_W 0x57
#define KEY_SHIFT_E 0x45
#define KEY_SHIFT_R 0x52
#define KEY_SHIFT_T 0x54
#define KEY_SHIFT_Y 0x59
#define KEY_SHIFT_U 0x55
#define KEY_SHIFT_I 0x49
#define KEY_SHIFT_O 0x4F
#define KEY_SHIFT_P 0x50
#define KEY_SHIFT_LBRACKET 0x7B  /* {[ */
#define KEY_SHIFT_RBRACKET 0x7D  /* }] */
#define KEY_SHIFT_A 0x41
#define KEY_SHIFT_S 0x53
#define KEY_SHIFT_D 0x44
#define KEY_SHIFT_F 0x46
#define KEY_SHIFT_G 0x47
#define KEY_SHIFT_H 0x48
#define KEY_SHIFT_J 0x4A
#define KEY_SHIFT_ENTER 0x1c
#define KEY_SHIFT_SPACE 0x39
#define KEY_SHIFT_UP 0x48
#define KEY_SHIFT_LEFT 0x4b
#define KEY_SHIFT_RIGHT 0x4d
#define KEY_SHIFT_DOWN 0x50

int keyboard_init(void);
uint16_t keyboard_read(void);

#endif
