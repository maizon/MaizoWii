#include "nav.h"
int maizo_nav_move(int current,int count,int columns,int horizontal,int direction){
 if(count<=0)return 0;
 if(current<0)current=0;
 if(current>=count)current=count-1;
 if(columns<1)columns=1;
 if(direction==0)return current;
 if(horizontal){
  if(columns==1)return current;
  int row=current/columns;
  int row_start=row*columns;
  int row_len=count-row_start;
  if(row_len>columns)row_len=columns;
  int col=current-row_start;
  return row_start+(col+row_len+(direction<0?-1:1))%row_len;
 }
 if(columns==1)return (current+count+(direction<0?-1:1))%count;
 int rows=(count+columns-1)/columns;
 int oldrow=current/columns;
 int col=current%columns;
 int row=(oldrow+rows+(direction<0?-1:1))%rows;
 int next=row*columns+col;
 if(next>=count)next=count-1;
 return next;
}
int maizo_nav_scroll(int current,int scroll,int count,int columns,int visible){
 if(count<=0)return 0;
 if(columns<1)columns=1;
 if(visible<columns)visible=columns;
 if(current<0)current=0;
 if(current>=count)current=count-1;
 if(scroll<0)scroll=0;
 int maxscroll=(count-1)/columns*columns;
 if(scroll>maxscroll)scroll=maxscroll;
 scroll=(scroll/columns)*columns;
 if(current<scroll)scroll=(current/columns)*columns;
 if(current>=scroll+visible){
  int rows=visible/columns;
  if(rows<1)rows=1;
  scroll=(current/columns-rows+1)*columns;
 }
 if(scroll<0)scroll=0;
 return scroll;
}
