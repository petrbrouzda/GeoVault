
const char * mnozneCislo( int n, const char * jedna, const char * dve, const char * vice ) {
  switch(n) {
    case 1: 
      return jedna;
    
    case 2: 
    case 3: 
    case 4: 
      return dve;

    default: 
      return vice;
  }
}

