#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <assert.h>
#include <algorithm>

int AddHistroyRecord(const char *file)
{
  sqlite3 *db;
  char *zErrMsg = 0;
  int rc,i;
  FILE *fp = NULL;
  char date[20], ball[7], sqlRequest[200];

  if((fp = fopen(file, "r")) == NULL){
      fprintf(stderr, "Can't open lottery history file \n");
      return 1;
  }
  rc = sqlite3_open("../database/MalkovModel", &db);
  if( rc ){
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }
  while(!feof(fp)){
    fscanf(fp, "%10s",date);
    for(i=0;i<7;++i) fscanf(fp, "%d", (int)(ball + i));
    sprintf(sqlRequest,
         "insert into HistoryRecord values (\"%10s\", %d, %d, %d, %d, %d, %d, %d);",date,\
        ball[0],ball[1],ball[2],ball[3],ball[4],ball[5],ball[6]);
    rc = sqlite3_exec(db, sqlRequest, NULL, 0, &zErrMsg);
    if( rc!=SQLITE_OK ){
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    }
  }
  fclose(fp);
  sqlite3_close(db);
  return 0;
}
//------------------------------------------------------------------------------------
int matrix[16][16];
char pre,next;
int gainMin = 0, lossMin = 0;
int gainMax = 0, lossMax = 0;
int num = 0;
//-----------------------------------------------------------------------------------
static int callback(void *NotUsed, int n, char **argv, char **azColName){
    int j;
    num += n;

    assert (n==1);
    next = atoi(argv[0]) - 1;
    
    if(num > 1000)
    {
        /*calc money*/
        int min = 10000, minIndex = -1;
        int max = -1, maxIndex = -1;
        for(j=0; j<16; j++)
        {
            if(min >  matrix[pre][j])
            {
                min = matrix[pre][j];
                minIndex = j;
            }
            else if(max < matrix[pre][j])
            {
                max = matrix[pre][j];
                maxIndex = j;
            }
        }
        if(next == minIndex)
        {
            ++gainMin;
        }
        else 
        {
            ++lossMin;
        }

        if(next == maxIndex) ++gainMax;
        else ++lossMax;
    }
    /*modify current status*/
    ++matrix[pre][next];
    pre = next;
    return 0;
}
int StatisticBlue()
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc,i,j;
    char sqlRequest[100]="select blue from HistoryRecord;";

    rc = sqlite3_open("../database/MalkovModel", &db);
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    for(i=0;i<16;++i)
        for(j=0;j<16;++j) matrix[i][j] = 0;
    pre = 0;
    rc = sqlite3_exec(db, sqlRequest, callback, 0, &zErrMsg);
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_exec(db, "delete from Blue;", NULL, 0, &zErrMsg);
    for(i=0;i<16;++i){
        sprintf(sqlRequest,
            "insert into Blue values(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d);",i+1,\
            matrix[i][0],  matrix[i][1],  matrix[i][2],  matrix[i][3],
            matrix[i][4],  matrix[i][5],  matrix[i][6],  matrix[i][7],
            matrix[i][8],  matrix[i][9],  matrix[i][10], matrix[i][11],
            matrix[i][12], matrix[i][13], matrix[i][14], matrix[i][15]);
        sqlite3_exec(db, sqlRequest, NULL, 0, &zErrMsg);
    }
    printf("Num %d\n", num);
    printf("Min loss %d gain %d result %d\n", lossMin, gainMin, lossMin*-2 + gainMin*5);
    printf("Max loss %d gain %d result %d\n", lossMax, gainMax, lossMax*-2 + gainMax*5);
    return 0;
}
//----------------------------------------------------------------------------------------------
void PredictMax(int blue, int *red)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc,nRow, nColumn, i, j, max = -1, maxIndex = -1;
    char sqlRequest[100];
    char **dbResult;
    int redNext[33], temp[33];

    //open the database
    rc = sqlite3_open("../database/MalkovModel", &db);
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return ;
    }

    //query for blue info
    sprintf(sqlRequest, "select * from Blue where pre = %d;", blue);
    rc = sqlite3_get_table(db, sqlRequest, &dbResult, &nRow, &nColumn, &zErrMsg);
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    //take hte max index
    for(i=0; i<16; ++i)
    {
        printf("%s ", dbResult[i+18]);
        rc = atoi(dbResult[i+18]);
        if(rc > max)
        {
            max = rc;
            maxIndex = i;
        }
    }
    printf("\n");

    for(j=0; j<33; ++j) redNext[j] = 0;
    //query for red info
    for(i=0; i<6; ++i)
    {
        sprintf(sqlRequest, "select * from Red where pre = %d;", red[i]);
        rc = sqlite3_get_table(db, sqlRequest, &dbResult, &nRow, &nColumn, &zErrMsg);

        for(j=0; j<33; ++j)
        {
            rc = atoi(dbResult[j+35]);
            redNext[j] += rc;
        }
    }
    //take the max 6 index
    for(i=0; i<33; ++i)
    {
        temp[i] = redNext[i];
        printf("%d:%d ", i, redNext[i]);
    }
    printf("\npredict red: ");
    std::sort(temp, temp + 33);
    for(i=0; i<6; ++i)
        for(j=0; j<33; ++j)
        {
            if(redNext[j] == temp[i])
            {
                printf("%d ", j+1);
                break;
            }
        }
    printf("\n blue %d\n", maxIndex+1);
}
//----------------------------------------------------------------------------------------------
int StatisticRed()
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc,i,j,k,nRow, nColumn,index;
    char sqlRequest[1024]="select * from HistoryRecord;";
    char **dbResult;
    int matrix[33][33],pre[6],next[6];

    rc = sqlite3_open("../database/MalkovModel", &db);
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    for(i=0;i<33;++i)
        for(j=0;j<33;++j) matrix[i][j] = 0;
    for(i=0;i<6;++i) pre[i]=0;
    rc = sqlite3_get_table(db, sqlRequest, &dbResult, &nRow, &nColumn, &zErrMsg);
    if( rc!=SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    index = nColumn;//第一行是字段名称 == 9 
    for(i=0;i<nRow;++i){//每一行
        ++index;//掠过主键
        for(j=0;j<6;++j){//当前每个数字
            next[j] = atoi(dbResult[index++]) - 1;
            for(k=0;k<6;++k){//上期的每个数字
                ++matrix[pre[k]][next[j]];
            }
            pre[j] = next[j];
        }
        ++index;//掠过blue
    }
    sqlite3_exec(db, "delete from Red;", NULL,0, &zErrMsg);
    for(i=0;i<33;++i){
        sprintf(sqlRequest,
            "insert into Red values(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d);",i+1,\
            matrix[i][0],  matrix[i][1],  matrix[i][2],  matrix[i][3],
            matrix[i][4],  matrix[i][5],  matrix[i][6],  matrix[i][7],
            matrix[i][8],  matrix[i][9],  matrix[i][10], matrix[i][11],
            matrix[i][12], matrix[i][13], matrix[i][14], matrix[i][15],
            matrix[i][16], matrix[i][17], matrix[i][18], matrix[i][19],
            matrix[i][20], matrix[i][21], matrix[i][22], matrix[i][23],
            matrix[i][24], matrix[i][25], matrix[i][26], matrix[i][27],
            matrix[i][28], matrix[i][29], matrix[i][30], matrix[i][31], matrix[i][32]);
        sqlite3_exec(db, sqlRequest, NULL, 0, &zErrMsg);
    }
    return 0;
}
//----------------------------------------------------------------------------------------------
int main(int argc, char **argv){
    int i;
    int preBlue;
    int preRed[6];
//    AddHistoryRecord("../lotterydata/HistoryData");

    StatisticBlue();
    StatisticRed();

    printf("input previous number:\n blue ");
    scanf("%d", &preBlue);
    printf("red ");
    for(i=0; i<6; ++i)
        scanf("%d", preRed + i);
    PredictMax(preBlue, preRed);

    return 0;
}
