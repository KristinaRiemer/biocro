/*
 * LeafOptics.cpp
 *
 *  Created on: 2012-11-23
 *      Author: Administrator
 */

#include "LeafOptics.h"
#include "math.h"
#include "Maths.h"
#include "Constants.h"
#include "iostream"
#include "Constants.h"

LeafOptics::LeafOptics() {
	// TODO Auto-generated constructor stub

}

LeafOptics::~LeafOptics() {
	// TODO Auto-generated destructor stub
}
Vector3D
LeafOptics::get_reflect_dir(Vector3D L, Vector3D N){
	double e = 0;
	double fr = 0;
	Vector3D r;// = new Vector3D;
	int xxx = 0;
	while (true){
		xxx++;
	//				cout<<"in get_reflect_dir: -> Ntheta= "<<Ntheta<<" Nphi="<<Nphi<<endl;
					randReflectRayDir(N, r);// randomize a direction above the face with Normal N
					fr = getfr(650,r,L,N);
	//				cout<<"in get_reflect_dir: -> fr= "<<fr<<endl;
				//	System.out.println("L: "+L[0]+", "+L[1]+", "+L[2]);
					e = rand()*invRAND_MAX;
	//				cout<<"in get_reflect_dir: -> e= "<<e<<endl;
				//	System.out.println("fr1: "+fr+"  e1: "+e);
					if (e<fr){
						//System.out.println(fr);
//						cout<<"rand Times: "<<xxx<<endl;
						break;
					}
	}
	return r;
}

// this is new method for randomizing reflect light, not probobility, but use reflectance (fr as proportion of refelct light energy). 2014-06-30
void 
LeafOptics::get_reflect_dir_2(Ray ray, Triangle* triangle_ptr, vector<Ray*> &scatter_rays, double ignor_Photon_Flux_threashold){
	double e = 0;
	double fr1,fr2,fr3,fr4,fr5, fra ;
	Vector3D r1, r2, r3, r4, r5;// = new Vector3D;
//	int xxx = 0;

	triangle_ptr->compute_normal(); // ??? delete?QF 2014.2.18
	Vector3D normal_triangle(triangle_ptr->normal);

	if (normal_triangle * ray.d > 0)
		normal_triangle = -normal_triangle;

	double pf = ray.photonFlux2  * triangle_ptr->kLeafReflectance; // total reflect rays energy, should be proportional divided to n reflect rays

//	cout <<"ray.photonFlux2: "<< ray.photonFlux2 << "triangle_ptr->kLeafReflectance: " << triangle_ptr->kLeafReflectance << endl;

	randReflectRayDir(normal_triangle, r1);// randomize a direction above the face with Normal N
	fr1 = getfr(650, r1, -ray.d, normal_triangle);
	randReflectRayDir(normal_triangle, r2);// randomize a direction above the face with Normal N
	fr2 = getfr(650, r2, -ray.d, normal_triangle);
	randReflectRayDir(normal_triangle, r3);// randomize a direction above the face with Normal N
	fr3 = getfr(650, r3, -ray.d, normal_triangle);
	randReflectRayDir(normal_triangle, r4);// randomize a direction above the face with Normal N
	fr4 = getfr(650, r4, -ray.d, normal_triangle);
	randReflectRayDir(normal_triangle, r5);// randomize a direction above the face with Normal N
	fr5 = getfr(650, r5, -ray.d, normal_triangle);

	fra = fr1 + fr2 +fr3 + fr4 + fr5;
	
	if (pf > ignor_Photon_Flux_threashold){
		scatter_rays.push_back(new Ray(triangle_ptr->hit_point, r1, pf*fr1 / fra));
		scatter_rays.push_back(new Ray(triangle_ptr->hit_point, r2, pf*fr2 / fra));
		scatter_rays.push_back(new Ray(triangle_ptr->hit_point, r3, pf*fr3 / fra));
		scatter_rays.push_back(new Ray(triangle_ptr->hit_point, r4, pf*fr4 / fra));
		scatter_rays.push_back(new Ray(triangle_ptr->hit_point, r5, pf*fr5 / fra));
	}
	

//	return r;
}

Vector3D
LeafOptics::get_transmit_dir(Vector3D L, Vector3D N){
	Vector3D t;// = new Vector3D;
	randThroughRayDir(N, t);
	return t;
}

void
LeafOptics::randReflectRayDir (Vector3D N, Vector3D & r){ // direction vector

	while(true){
		double x = rand()*invRAND_MAX*2-1;  // 0-1        //random points in the cube
		double y = rand()*invRAND_MAX*2-1;  // 0-1
		double z = rand()*invRAND_MAX*2-1;  // 0-1
		if (x*x+y*y+z*z>1)                            //points should be inside of the sphere
			continue;
		if (x*N.x + y*N.y + z*N.z<=0)                 //points should be in the Hemisphere "same" side with normal
			continue;
		r.x = x; r.y = y; r.z = z;                    //sample vector
		r.normalize();                                //normalize
		break;
	}
}
void
LeafOptics::randThroughRayDir (Vector3D N, Vector3D & r){ // direction vector

	while(true){
		double x = rand()*invRAND_MAX*2-1;  // 0-1        //random points in the cube
		double y = rand()*invRAND_MAX*2-1;  // 0-1
		double z = rand()*invRAND_MAX*2-1;  // 0-1
		if (x*x+y*y+z*z>1)                            //points should be inside of the sphere
			continue;
		if (x*N.x + y*N.y + z*N.z>=0)                 //points should be in the Hemisphere "opposite" side with normal
			continue;
		r.x = x; r.y = y; r.z = z;                    //sample vector
		r.normalize();                                //normalize
		break;
	}
}
double
LeafOptics::getfr (double hv_wave_length, Vector3D V, Vector3D L, Vector3D N){

		double s = BRDF_s;
		double F0 = BRDF_F0;
		double Rd = BRDF_Rd;
		double m = BRDF_m;
//		cout<<"in getfr: -> V= "<<V.x<<" "<<V.y<<" "<<V.z<<endl;
//		cout<<"in getfr: -> L= "<<L.x<<" "<<L.y<<" "<<L.z<<endl;
//		cout<<"in getfr: -> N= "<<N.x<<" "<<N.y<<" "<<N.z<<endl;
//		cout<<"in getfr: -> s="<<s<<" F0="<<F0<<" Rd="<<Rd<<" m="<<m<<endl;
		//H vector
		Vector3D H = vMidLine(L,V);
//		cout<<"in getfr: -> H= "<<H.x<<" "<<H.y<<" "<<H.z<<endl;
		//G
		double G1 = 2*(N*H)*(N*V)/(V*H);
		double G2 = 2*(N*H)*(N*L)/(V*H);
		double G  = min(1, min(G1,G2));
//		cout<<"in getfr: -> G1= "<<G1<<" G2="<<G2<<" G="<<G<<endl;
	//	double G1 = 2*vM(N,H)*vM(N,V)/vM(V,H);
	//	double G2 = 2*vM(N,H)*vM(N,L)/vM(V,H);
	//	double G = Math.min(1,Math.min(G1, G2));

		//D
		double a = vAngle(N,H); //Change....

		double D = 1/(m*m*pow(cos(a), 4)) * exp(-pow(tan(a)/m, 2));
		//F
		double a2 = vAngle(N,L);
		double F = F0 + (1-F0)*pow(1-cos(a2), 5);
//		cout<<"in getfr: -> a= "<<a<<" D="<<D<<" a2="<<a2<<" F="<<F<<endl;

	//	System.out.println("s: "+s);
		double fr = s*F/PI*D*G/((N*L)*(N*V)) + (1-s)*Rd;
	//	double fr = s*F/Math.PI*D*G/(vM(N,L)*vM(N,V)) + (1-s)*Rd;

	//	System.out.println(G+"\t"+a+"\t"+a2+"\t"+D+"\t"+fr+"\t"+F0+"\t"+F);
		return fr;

}

Vector3D
LeafOptics::vMidLine (Vector3D A, Vector3D B){
	Vector3D C = A+B;
	C.normalize();
	return C;
}

double
LeafOptics::vAngle (Vector3D A, Vector3D B){
	    double angle = acos((A*B)/(A.length()*B.length()));
		return angle;
}

