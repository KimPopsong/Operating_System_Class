/*
 * GeekOS C code entry point
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2004, Iulian Neamtiu <neamtiu@cs.umd.edu>
 * $Revision: 1.51 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/bootinfo.h>
#include <geekos/string.h>
#include <geekos/screen.h>
#include <geekos/mem.h>
#include <geekos/crc32.h>
#include <geekos/tss.h>
#include <geekos/int.h>
#include <geekos/kthread.h>
#include <geekos/trap.h>
#include <geekos/timer.h>
#include <geekos/keyboard.h>


/*
 * Kernel C code entry point.
 * Initializes kernel subsystems, mounts filesystems,
 * and spawns init process.
 */

#define ASCII_ENTER 0x0d	//enter값 미리 설정 <<줄바꿈
#define INIT_LINE   12		//어느정도 치고나면 줄바꿈되는 정도

static void project_start(ulong_t arg);

void Main(struct Boot_Info* bootInfo)
{
    Init_BSS();			
	Init_Screen();		
    Init_Mem(bootInfo);
    Init_CRC32();
    Init_TSS();
    Init_Interrupts();
    Init_Scheduler();
    Init_Traps();
    Init_Timer();
    Init_Keyboard();


    Set_Current_Attr(ATTRIB(BLACK, GREEN|BRIGHT));
    Print("Hello From Insoo!\n"); //수행자 이름 출력
    Set_Current_Attr(ATTRIB(BLACK, GRAY));

	//커널 스레드로 project_start함수 수행 
	Start_Kernel_Thread(project_start,0,PRIORITY_NORMAL, true );

    /* Now this thread is done. */
    Exit(0);
}

static void project_start(ulong_t arg)
{
	int cs_row, cs_col; 	// 커서의 위치
	int cs_release; 		// 키가 떼어졌나의 여부
	int init_page_flag = 1; // 첫페이지를 보호하기 위해서
	int array[100]; 		// 행의 개수
     
	Keycode key_input; // 입력받은 키를 담을 ushort_t 형 자료형
	// 상위 6bit는 flag가 들어가고 하위 8bit는 ASCII값이 들어감

    // 글자의 색을 빨간색으로 바꿈
    Set_Current_Attr(ATTRIB(BLACK, RED|BRIGHT));
    // 화면에 문자열을 출력
    Print("\n=================================================");
    Print("\n                start Project                    ");
    Print("\n=================================================\n\n");
    // 다시 (원래대로) 글자의 색을 회색으로 바꿈
    Set_Current_Attr(ATTRIB(BLACK, GRAY));

    while(1){	//계속적으로 키보드 인풋을 받기 위해서 while(1)
        Get_Cursor(&cs_row, &cs_col);		//스크린으로부터 커서의 위치를 구함
 
        /* KeyCode값은 키보드가 눌렸다가(KEY_SPECIAL_FLAG)
           떼어졌을 때(KEY_RELEASE_FLAG)의 2번을 인식
           입력된 키 코드값을 반환 (2개 다 입력되있는 상태) */
        key_input = Wait_For_Key();

        /*	Wait_For_Key()로 받아온 ASCII값과 KEY_RELEASE_FLAG을 AND연산하여
           	release가 0이면 키가 눌러졌음을 의미 */
        cs_release = key_input & KEY_RELEASE_FLAG;

        if(cs_release == KEY_RELEASE_FLAG)
            key_input = KEY_RELEASE_FLAG;

        // 커서가 마지막 줄에 다달았다면

        if(cs_row == (NUMROWS-1)){
            // 화면을 비우고
            Clear_Screen();
            init_page_flag = 0; // 첫페이지를 보호하지 않음
            cs_row = 0;
            cs_col = 0;
            // 커서를 왼쪽 상단에 위치 시킴
            Put_Cursor(cs_row,cs_col);
        }else  

        // 커서가 오른쪽 제일 끝에 다달았다면

        if (cs_col == (NUMCOLS-1)) {
            // 해당 행(row)에 마지막 열(col)의 위치를 저장
            array[cs_row] = (cs_col-1);
            // 커서를 다음 행의
            ++cs_row;
            // 제일 좌측에
            cs_col=0;
            // 위치시킴
            Put_Cursor(cs_row,cs_col);
        }
        // Processing : Value Of KeyBoard Input
        switch( key_input )
        {
            case KEY_RELEASE_FLAG:  // Release
                break;
            case KEY_KPUP:          // 위 - 윗행으로 감
                Put_Cursor(cs_row-1, cs_col);
                break;

            case KEY_KPDOWN:        // 아래 - 아래행으로 감
                Put_Cursor(cs_row+1, cs_col);
                break;
         
            case KEY_KPLEFT:        // 왼쪽 - 왼쪽열로 감
                if( cs_col==0 ){		// 첫번째 열이면 이전행 마지막으로
                    Put_Cursor( --cs_row, NUMCOLS);
				}
                else
                    Put_Cursor(cs_row, --cs_col);
                break;
         
            case KEY_KPRIGHT:       // 오른쪽 - 오른쪽열로 감
                if( cs_col==NUMCOLS )	//마지막 열이면 다음행 처음으로
                    Put_Cursor( ++cs_row, 0);
                else
                    Put_Cursor(cs_row, ++cs_col);
                break;
 
            case ASCII_ENTER:       // Enter - 줄바꿈
                if( cs_col<=0 )		// 아무것도 안쓰고 엔터면 그냥 0열
                    array[cs_row]=0;
                else				// 뭐라도 쓴채로 엔터면 현재열 저장
                    array[cs_row]=cs_col-1;
                if( cs_row==NUMROWS )	// 행 한계에 다다르면 더이상 안함
                    break;
                cs_col=0;			// 행이 바뀌었으니 다시 첫번째 열부터 시작해볼까
                ++cs_row;			// 행 하나 증가시켜보자 
                Put_Cursor(cs_row,cs_col);	// 바뀐 행과 열에따라 커서 재위치
                break;
 
            case ASCII_BS:          // BackSpace
                if( (init_page_flag==1) && (cs_row==INIT_LINE) && (cs_col==0) )
                    break;
                if( (cs_row==0) && (cs_col==0) ) // 아무것도 없으면 지울게 없죠
                    break;
                else
                {  
                    if( cs_col==0 )			//첫번째 열이면 이전행 마지막껄 지워야함
                    {
                        --cs_row;
                        cs_col=array[cs_row];    //이전행의 마지막 열 값 받아옴
                    }
                    else
                        --cs_col;			// 위에 조건들이아니면 그냥 그자리에서 왼쪽
                 
                    Put_Cursor(cs_row, cs_col);
                    Print(" ");	// 아무것도 없는 것으로 보여줌
                    Put_Cursor(cs_row, cs_col);
                }
                break;
 
            case (KEY_CTRL_FLAG|'d'):   // Ctrl+D - 컨트롤+D를 이렇게 쓰나봄
                {
                    Print("\n=================================================");
                    Print("\n                   Thank You ^^                  ");
                    Print("\n=================================================\n");
                    Exit(1);			//1번형식 exit - qemu를 종료시키진않음
                }
         
            default :      
                Print("%c", key_input);  
				//Print("%x ,%x", cs_row, cs_col);        
			}	
    }
}  


