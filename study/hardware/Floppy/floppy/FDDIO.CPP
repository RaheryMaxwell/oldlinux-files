#ifdef __cplusplus
    #define __CPPARGS ...
#else
    #define __CPPARGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <dos.h>

#define CYL 0

typedef struct _DPT_
{
  unsigned char srt_hut;
  unsigned char dma_hlt;
  unsigned char motor_w;
  unsigned char sec_size;
  unsigned char eot;
  unsigned char gap_rw;
  unsigned char dtl;
  unsigned char gap_f;
  unsigned char fill_char;
 unsigned char hst;
 unsigned char mot_start;
} DPT;

DPT far *get_dpt(void);
void fdc_out(unsigned char byte);
int  fdc_inp(void);
void int_wait(void);
void dma_init(void far *);
void tdelay(int cnt);
void interrupt IRQ6 (__CPPARGS);
void interrupt (*oldIRQ6) (__CPPARGS);

char buffer[512];

static  int IRQ=0;

void main(void)
{
  unsigned i;
  long l;
  char status[7], main_status;
  DPT _far *fdpt;
  FILE *sect;

  printf("\n\n����� � ����஫��஬ ����"
	 "\n  (C)�஫�� �., 1995\n");

  oldIRQ6 = _dos_getvect (8+6);
  _dos_setvect (8+6, IRQ6);

  // ���뢠�� 䠩�, � ����� �㤥� �����뢠��
  // ᮤ�ন��� ᠬ��� ��ࢮ�� ᥪ�� ��᪥��
  sect = fopen("!sector.dat","wb+");

  // ��⠭�������� 㪠��⥫� �� ⠡����
  // ��ࠬ��஢ ��᪥��
  fdpt = get_dpt();

  // ����砥� ���� ��᪮���� �:
  // ��। �⨬ ࠧ�蠥� ���뢠���
  _enable();
  outp(0x3F2, 0x1C);

  // �믮��塞 ����প� ��� ࠧ���� �����⥫�
  tdelay(18);

  // �����뢠�� ᮤ�ন��� ॣ���� �᭮�����
  // ���ﭨ� ����஫���
  printf("���� ����祭.\t\t");
  printf("�᭮���� ���ﭨ�: %02.2X\n",inp(0x3F4));

  // ��। �⥭��� ᥪ�� ����室��� ��⠭�����
  // ������� �� �㦭�� ��஦��, � ��襬 ��砥 ��
  // ��஦�� � ����஬ CYL

  // �뤠�� ����஫���� ������� "����"
  fdc_out(0xf);

  // ��� ������� "����" �ॡ���� ��� ���� ��ࠬ��஢:
  // ����� �������/����� ������⥫� � ����� ��஦��.
  // �� ࠡ�⠥� � �㫥��� �������� ������⥫� �:,
  // ���⮬� ���� ��ࠬ��� ࠢ�� 0, ��ன - CYL
  fdc_out(0);
  fdc_out(CYL);

  // �����뢠�� ᮤ�ন��� ॣ���� �᭮�����
  // ���ﭨ� ����஫���
  printf("\n<<<����>>> \t\t");
  printf("�᭮���� ���ﭨ�: %02.2X\n",inp(0x3F4));

  // ������� ���뢠��� �� �����襭�� ����樨
  int_wait();

  // ����প� ��� ����樮��஢���� �������
  tdelay(1);

  // ��� �஢�ન १���� �믮������ �������
  // "����" �뤠�� ����஫���� �������
  // "�⥭�� ���ﭨ� ���뢠���"

  // �뢮��� ᮤ�ন��� ॣ���� ���ﭨ�
  // ST0  � ����� ��஦�� ��᫥ �믮������ �������
  // "����" PCN
  fdc_out(0x8);
  printf("����ﭨ� ���뢠���:\t");
  printf(" ST0: %02.2X, \t", fdc_inp());
  printf("PCN: %02.2X\n", fdc_inp());

  // ��� ����� ��㡮��� �������⨪� ���ﭨ�
  // ����஫��� �뤠�� ����஫���� �������
  // "�⥭�� ���ﭨ� ������⥫�", �뢮���
  // ᮤ�ন��� ॣ���� ���ﭨ� ST3
  fdc_out(4);
  fdc_out(0);
  printf("����ﭨ� ������⥫�:\t ST3: %02.2X\n",fdc_inp());

  // ��⠭�������� ᪮���� ��।�� ������ 500 �����/�
  outp(0x3F7, 0);

  // ���樠������ ������ ��אַ��
  // ����㯠 � �����
  dma_init((void far *)buffer);

  // �뤠�� ������� "�⥭�� ������"
  fdc_out(0x66);
  fdc_out(0x0);     // ������⥫� 0, ������� 0

  fdc_out(CYL);     // 樫���� CYL
  fdc_out(0);       // ������� 0
  fdc_out(1);       // ����� ᥪ�� - 1

  // ��।��� ����஫���� �孨�᪨� ��ࠬ����
  // ��᪮����, ��६ �� �� ⠡���� ��ࠬ��஢ ��᪥��.
  // �� ⠪�� ��ࠬ����:
  //    - ࠧ��� ᥪ��;
  //    - ����� ��᫥����� ᥪ�� �� ��஦��;
  //    - ࠧ��� �஬���⪠;
  //    - �᫮ ���뢠����/�����뢠���� ���⮢
  fdc_out(fdpt->sec_size);
  fdc_out(fdpt->eot);
  fdc_out(fdpt->gap_rw);
  fdc_out(fdpt->dtl);

  // ������� ���뢠��� ��᫥ �����襭�� ����樨
  int_wait();

  // ���뢠�� � �뢮��� �� ��࠭ ����� १����
  // ����樨 "�⥭�� ������"
  printf("\n<<<�⥭�� ᥪ��>>> \n");
  printf("   ����� ���ﭨ� (ST0,ST1,ST2,C,H,R,N):\n");

  for(i=0; i<7; i++) printf("%02.2X\t", (char) fdc_inp());
  printf("\n");

  // �뢮��� ᮤ�ন��� ��⠭���� ᥪ�� � 䠩�
  for(i=0; i<512; i++) fputc(buffer[i],sect);
  fclose(sect);

  // �몫�砥� ����
  outp(0x3F2, 0xC);

  _dos_setvect (8+6, oldIRQ6);
}

// �뢮� ���� � ����஫��� ��᪮����
void fdc_out(unsigned char parm)
{
  asm mov   dx,3F4h     // ���� �᭮����� ���ﭨ�
loop_fdc_out:

  asm in    al,dx
  asm test  al,80h      // �஢��塞 ��⮢�����
  asm jz loop_fdc_out   //   ����஫���

  asm inc   dx          // �뢮��� ���� � ���� ������
  asm mov   al, parm    //   ����஫���
  asm out   dx, al
}

// ���� ���� �� ���� ������ ����஫��� ��᪮����
int fdc_inp(void)
{
  asm mov   dx,3F4h     // ���� �᭮����� ���ﭨ�
loop_fdc_inp:
  asm in    al,dx
  asm test  al,80h      // �஢��塞 ��⮢�����
  asm jz loop_fdc_inp   //   ����஫���

  asm inc   dx          // �������� ���� �����뢠��
  asm in    al, dx      // � ॣ���� AX
  return _AL;
}

// �������� ���뢠��� �� ����஫���
void int_wait (void) {
  _enable();
  while (IRQ==0) {};
  IRQ = 0;
}

void interrupt IRQ6 (__CPPARGS) {
  IRQ = 1;
  outportb (0x20, 0x20);
}

// ���樠������ ������ ��אַ�� ����㯠 � �����
void dma_init(void far *buf)
{
  unsigned long f_adr;
  unsigned sg, of;

  // ����塞 24-ࠧ�來� ���� ���� ��� ������
  f_adr = ((unsigned long)FP_SEG(buf) << 4)
    + (unsigned long)FP_OFF(buf);

  // ���饯�塞 ���� �� ����� ��࠭���
  // � ᬥ饭��
  sg = (f_adr >> 16) & 0xff;
  of = f_adr & 0xffff;

  // �� �६� �ணࠬ��஢���� ����஫��� ��אַ��
  // ����㯠 ����頥� ���뢠���
  _disable();
  asm mov   al,46h   // ������� �⥭�� ������ ��
                     // ����஫��� ����

  asm out   12,al    // ���� �ਣ���-㪠��⥫� ����
                     // ��� ࠡ��� � 16-ࠧ�來묨 ���⠬�.
		     // ������騩 ����, �뢮���� � 16-ࠧ�來�
		     // ���� �㤥� ������஢�����
		     // ��� ����訩

  asm out   11,al    // ��⠭���� ०��� ����஫��� ���

  asm mov   ax,of    // ���饭�� ����, ����訩 ����
  asm out   4,al
  asm mov   al,ah    // ���饭�� ����, ���訩 ����
  asm out   4,al

  asm mov   ax,sg    // ����� ��࠭���
  asm out   81h,al

  asm mov   ax,511   // ����� ��।������� ������
  asm out   5,al
  asm mov   al,ah
  asm out   5,al

  asm mov   al,2     // ��������஢�� ������ 2 ����஫��� ���
  asm out   10,al

  // ���樠������ ����஫��� �����祭�,
  // ࠧ�蠥� ���뢠���.
  _enable();
}

/**
* get_dpt
*
* ���᫨�� ���� ⠡���� ��ࠬ��஢ ��᪥��
*
* �㭪�� �����頥� 㪠��⥫� �� ⠡����
* ��ࠬ��஢ ��᪥��
*
**/

DPT far *get_dpt(void)
{
  void far * far *ptr;

  ptr = (void far * far *)MK_FP(0x0, 0x78);
  return(DPT far*)(*ptr);
}

/**
* tdelay
*
* ��ନ஢���� �६����� ����প� �� �����
* ⠩���.
*
* � ����⢥ ��ࠬ��� �㭪樨 ��।�����
* ���⥫쭮��� ����প� � ������⢥ ���뢠���,
* ����㯠��� �� ⠩��� (⠩��� ��������
* � ���� ᥪ㭤� �ਬ�୮ 18 ���뢠���)
*
**/

void tdelay(int cnt)
{
  asm push bx
  asm push dx
  asm push si

  asm mov si, cnt
  asm mov ah, 0
  asm int 1ah
  asm mov bx, dx
  asm add bx, si

delay_loop:
  asm int 1ah
  asm cmp dx, bx
  asm jne delay_loop

  asm pop si
  asm pop dx
  asm pop bx
}
