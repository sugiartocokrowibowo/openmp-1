/* Первый алгоритм
 *
 * Решение СЛАУ методом Гаусса. Распределение данных - горизонтальными полосами.
 * (Запуск задачи на 8-ми компьютерах).
 */

   #include<stdio.h>
   #include<mpi.h>
   #include<sys/time.h>
   #include<stdlib.h>

/* Каждая ветвь задает размеры своих полос матрицы MA и вектора правой части.
 * (Предполагаем, что размеры данных делятся без остатка на количество
 * компьютеров.) */
   #define M 400
   #define N 200
   #define tegD 1
   #define EL(x) (sizeof(x) / sizeof(x[0]))

/* Описываем массивы для полос исходной матрицы - MA и вектор V для приема
 * данных. Для простоты, вектор правой части уравнений присоединяем
 * дополнительным столбцом к матрице коэффициентов. В этом дополнительном
 * столбце и получим результат. */
double MA[N][M+1], V[M+1], MAD, R;

 

int main(int args, char **argv)
  { int size, MyP, i, j, v, k, d, p;
    int       *index, *edges;
    MPI_Comm   comm_gr;
    MPI_Status status;
    struct timeval tv1, tv2;
    int dt1;
    int reord = 1;

 /* Инициализация библиотеки */
    MPI_Init(&args, &argv);
 /* Каждая ветвь узнает размер системы */
    MPI_Comm_size(MPI_COMM_WORLD, &size);
 /* и свой номер (ранг) */
    MPI_Comm_rank(MPI_COMM_WORLD, &MyP);

 /* Выделяем память под массивы для описания вершин и ребер в топологии
  * полный граф */
    index = (int *)malloc(size * sizeof(int));
    edges = (int *)malloc(size*(size-1) * sizeof(int));

 /* Заполняем массивы для описания вершин и ребер для топологии
  * полный граф и задаем топологию "полный граф". */
    for(i = 0; i < size; i++)
      { index[i] = (size - 1)*(i + 1);
        v = 0;
        for(j = 0; j < size; j++)
          { if(i != j)
              edges[i * (size - 1) + v++] = j;
          }
      }
    MPI_Graph_create(MPI_COMM_WORLD, size, index, edges, reord, &comm_gr);

/* Каждая ветвь генерирует свою полосу матрицы A и свой отрезок вектора
 * правой части, который присоединяется дополнительным столбцом к A.
 * Нулевая ветвь генерирует нулевую полосу, первая ветвь - первую полосу
 * и т.д. (По диагонали исходной матрицы - числа = 2, остальные числа = 1). */
    for(i = 0; i < N; i++)
      { for(j = 0; j < M; j++)
          { if((N*MyP+i) == j)
              MA[i][j] = 2.0;
            else
              MA[i][j] = 1.0;
          }
        MA[i][M] = 1.0*(M)+1.0;
      }

 /* Каждая ветвь засекает начало вычислений и производит вычисления */
    gettimeofday(&tv1, (struct timezone*)0);

 /* Прямой ход */
 /* Цикл p - цикл по компьютерам. Все ветви, начиная с нулевой, последовательно
  * приводят к диагональному виду свои строки. Ветвь, приводящая свои строки
  * к диагональному виду, назовем активной, строка, с которой производятся
  * вычисления, так же назовем активной. */
    for(p = 0; p < size; p++)
      {
     /* Цикл k - цикл по строкам. (Все веиви "крутят" этот цикл). */
        for(k = 0; k < N; k++)
          { if(MyP == p)
              {
             /* Активная ветвь с номером MyP == p приводит свои строки к
              * диагональному виду.
              * Активная строка k передается ветвям, с номером большим чем MyP */
                MAD = 1.0/MA[k][N*p+k];
                for(j = M; j >= N*p+k; j--)
                  MA[k][j] = MA[k][j] * MAD;
                for(d = p+1; d < size; d++)
                  MPI_Send(&MA[k][0], M+1, MPI_DOUBLE, d, tegD, comm_gr);
                for(i = k+1; i < N; i++)
                  { for(j = M; j >= N*p+k; j--)
                      MA[i][j] = MA[i][j]-MA[i][N*p+k]*MA[k][j];
                  }
              }
         /* Работа принимающих ветвей с номерами MyP > p */
            else if(MyP > p)
              {  MPI_Recv(V, EL(V), MPI_DOUBLE, p, tegD, comm_gr, &status);
                 for(i = 0; i < N; i++)
                   { for(j = M; j >= N*p+k; j--)
                       MA[i][j] = MA[i][j]-MA[i][N*p+k]*V[j];
                   }
              }
          }        /* for k */
       }           /* for p */

 /* Обратный ход */
 /* Циклы по p и k аноалогичны, как и при прямом ходе. */
    for(p = size-1; p >= 0; p--)
      { for(k = N-1; k >= 0; k--)
          {
         /* Работа активной ветви */
            if(MyP == p)
              { for(d = p-1; d >= 0; d--)
                  MPI_Send(&MA[k][M], 1, MPI_DOUBLE, d, tegD, comm_gr);
                for(i = k-1; i >= 0; i--)
                  MA[i][M] -= MA[k][M]*MA[i][N*p+k];
              }
         /* Работа ветвей с номерами MyP < p */
            else
              { if(MyP < p)
                  { MPI_Recv(&R, 1, MPI_DOUBLE, p, tegD, comm_gr, &status);
                    for(i = N-1; i >= 0; i--)
                      MA[i][M] -= R*MA[i][N*p+k];
                  }
              }
          }                /* for k */
      }                    /* for p */

 /* Все ветви засекают время и печатают */
    gettimeofday(&tv2, (struct timezone*)0);
    dt1 = (tv2.tv_sec - tv1.tv_sec)*1000000 + tv2.tv_usec - tv1.tv_usec;
    printf("MyP = %d Time = %d\n", MyP, dt1);
 /* Все ветви печатают, для контроля, свои первые четыре значения корня */
    printf("MyP = %d %f %f %f %f\n",MyP,MA[0][M],MA[1][M],MA[2][M],MA[3][M]);
/* Все ветви завершают выполнение */
    MPI_Finalize();
    return(0);
  }
