void fixed(int *arr) {
  for (int i = 0; i < 16; i++) {
    arr[i] = i * 2;
  }
}

void dynamic(int n, int *arr) {
  for (int i = 0; i < n; i++) {
    arr[i] = 0;
  }
}

void nested(int A[8][8]) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      A[i][j] = i + j;
    }
  }
}

void large(int *arr) {
  for (int i = 0; i < 1024; i++) {
    arr[i] = 0;
  }
}
