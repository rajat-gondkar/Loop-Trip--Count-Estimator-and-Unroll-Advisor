void large_trip(float *a, float *b) {
  for (int i = 0; i < 1024; i++) a[i] = b[i] * 3.14f;
}
