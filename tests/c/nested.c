void nested(int A[8][8]) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      A[i][j] = i + j;
    }
  }
}
