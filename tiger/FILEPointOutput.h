// #included from rasterizeTiger.h

template<class T>
void FILEPointOutput<T>::setEnd(T* end) {
	end[0] = end[1] = (T)-1;
}

template<class T>
bool FILEPointOutput<T>::writePoint(uint64_t ubid, int px, int py) {
	if (firstout || (ubid != lastUbid)) {
		if (!firstout) {
			T end[2];
			setEnd(end);
			fwrite(end,sizeof(T),2,fout);
		} else {
			firstout = false;
		}
		fwrite(&ubid,sizeof(uint64_t),1,fout);
		lastUbid = ubid;
	}
#if 0
	assert( px >= 0 );
	assert( px < 0x7fff );
	assert( py >= 0 );
	assert( py < 0x7fff );
#endif
	T xy[2];
	xy[0]=px;
	xy[1]=py;
	fwrite(xy,sizeof(T),2,fout);
	return true;
}
template<class T>
bool FILEPointOutput<T>::flush() {
	if (!firstout) {
		T end[2];
		setEnd(end);
		fwrite(end,sizeof(T),2,fout);
		firstout = true;
	}
	fflush(fout);
	return true;
}
template<class T>
bool FILEPointOutput<T>::close() {
	flush();
	fclose(fout);
	fout = NULL;
	return true;
}
template<class T>
FILEPointOutput<T>::FILEPointOutput(FILE* out) : fout(out), lastUbid(0) {}
template<class T>
FILEPointOutput<T>::~FILEPointOutput() {
	if (fout != NULL) {
		close();
	}
}
