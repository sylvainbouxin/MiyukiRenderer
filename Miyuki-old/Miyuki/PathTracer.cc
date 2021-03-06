#include "PathTracer.h"
#include "Scene.h"
#include "vec.h"

using namespace Miyuki;
vec3 Miyuki::PathTracer::sampleLights(RenderContext & ctx)
{
	return sampleLights(ctx.ray, ctx.intersection, ctx.Xi);
}

vec3 Miyuki::PathTracer::sampleLights(const Ray & ray, Intersection & isct, Seed * Xi)
{
	vec3 color(0, 0, 0);
	//for (auto light : scene->lights) {
	auto light = scene->lights[nrand48(Xi) % scene->lights.size()];
	if (isct.object == light) {
		return  light->getMaterial().emittance;
	}
	auto p = light->randomPointOnObject(Xi);
	auto area = light->area();
	auto shadow = p - ray.o;
	auto distSqr = shadow.lengthSquared();
	shadow.normalize();
	Intersection vis;
	vis.exclude = isct.object;
	Ray shadowRay(ray.o , shadow);
	intersect(shadowRay, vis);
	if (vis.hit() && vis.object == light) {
		color += area / (distSqr + 0.001)
			* std::max(Float(0.0), -(vec3::dotProduct(shadow, vis.normal)))
			* light->getMaterial().emittance
			//* (1 - isct.object->getMaterial().reflectance(shadow * -1,isct.normal))
			* std::max(Float(0.0), (vec3::dotProduct(shadow, isct.normal)))
			*  light->getMaterial().emissionStrength;
	}
	//}
	return color * scene->lights.size();
}

void Miyuki::PathTracer::intersect(const Ray & ray, Intersection &intersection)
{
	intersection.reset();
	//bvh->intersect(ray, intersection);
	scene->accel->intersect(ray, intersection);
}

void Miyuki::PathTracer::prepare()
{
	camera = scene->camera;

}

vec3 Miyuki::PathTracer::trace(int x0, int y0)
{
	RenderContext ctx = scene->getRenderContext(x0, y0);
	return trace(ctx);
}
vec3 Miyuki::PathTracer::trace(RenderContext & ctx)
{
	while (ctx.depth < scene->option.maxDepth) {
		intersect(ctx.ray, ctx.intersection);
		if (!ctx.intersection.hit()) {
			break;
		}
		ctx.ray.o += ctx.ray.d * ctx.intersection.distance;
		auto object = ctx.intersection.object;
		auto& material = object->getMaterial();
		vec3 wo;
		vec3 rad;
		Float prob;
		BxDFType type = material.sample(ctx.sampler, ctx.ray.d, ctx.intersection.normal, wo, rad, prob);
		if (prob < eps)
			break;
		if (type == BxDFType::none)break;

		if (type == BxDFType::emission) {
			if (ctx.sampleEmit)
				ctx.color += ctx.throughput * rad / prob * material.emissionStrength;
			break;
		}
		ctx.throughput *= rad / prob;

		if (type == BxDFType::diffuse) {
			if (ctx.directLighting) {
				ctx.sampleEmit = false;
				auto direct = sampleLights(ctx);

				ctx.color += direct * ctx.throughput;
			}
			ctx.throughput *= pi;
		}
		else {
			ctx.sampleEmit = true;
		}
		ctx.ray.d = wo;



		const Float P = max(ctx.throughput);
		if (ctx.depth > scene->option.rrStartDepth && P < 1) {
			if (ctx.sampler.sample() < P) {
				ctx.throughput /= P;
			}
			else {
				break;
			}
		}
		ctx.depth++;
	}
	return ctx.color;
}
vec3 Miyuki::PathTracer::raycast(int x0, int y0)
{
	RenderContext ctx = scene->getRenderContext(x0, y0);
	int m = scene->option.maxDepth;
	while (ctx.depth < 1) {
		intersect(ctx.ray, ctx.intersection);
		if (!ctx.intersection.hit()) {
			ctx.color += scene->worldColor; break;
		}
		ctx.ray.o += ctx.ray.d * ctx.intersection.distance;
		return ctx.intersection.object->getMaterial().diffuse
			* std::max(Float(0.2),vec3::dotProduct(ctx.intersection.normal,vec3(0.2,1,0).normalized()))
			+ ctx.intersection.object->getMaterial().emittance;
		ctx.ray.o += ctx.ray.d * eps * 4;
		if (ctx.color.max() > 1)
			break;
	}
	return min(ctx.color, 10*vec3(3, 3, 3));
}

void Miyuki::PathTracer::render(Scene *s)
{
	scene = s;
	logger = s->getLogger();
	vec3 dim = scene->getDimension();
	int w = dim.x();
	int h = dim.y();
	parallelFor(0, w, [&](unsigned int i) {
		int x = i;
		for (int y = 0; y < h; y++) {
			scene->getScreen()[x + scene->w * y] *= scene->sampleCount;
			scene->getScreen()[x + scene->w * y] += min(3 * vec3(1, 1, 1), trace(x, y));
			scene->getScreen()[x + scene->w * y] /= 1 + scene->sampleCount;
		}
	});
}

void Miyuki::BDPT::intersect(const Ray & ray, Intersection &isct)
{
	isct.reset();
	scene->getAccel()->intersect(ray, isct);
}

void Miyuki::BDPT::traceLightPath(Seed*Xi, Path &path)
{
	Ray ray(vec3(0, 0, 0), vec3(0, 0, 0));
	vec3 radiance(0, 0, 0);
	vec3 refl(1, 1, 1);
	Intersection isct;
	{
		auto light = scene->lights[nrand48(Xi) % scene->lights.size()];
		ray.o = light->randomPointOnObject(Xi);
		auto n = light->getNormOfPoint(ray.o);
		ray.d = randomVectorInHemisphere(n, Xi);
		radiance = light->getMaterial().emittance * light->getMaterial().emissionStrength
			* light->area() * pi * 2* scene->lights.size();
		LightVertex v0;
		v0.G = 1;
		v0.pdf = v0.pdfSA = 1;
		v0.normal = n;
		v0.wo = ray.d;
		v0.wi = vec3(0, 0, 0);
		v0.object = light;
		v0.type = BxDFType::emission;
		v0.radiance = radiance;
		v0.throughput = refl;
		v0.hitpoint = ray.o;
		isct.exclude = light;
		path.emplace_back(v0);
	}
	
	LightVertex v;
	
	for (int depth = 0; depth < scene->option.maxDepth; depth++) {
		intersect(ray, isct);
		if (!isct.hit())
			break;
		ray.o += ray.d * isct.distance;
		auto object = isct.object;
		auto& material = object->getMaterial();
		auto& Kd = material.diffuse;
		auto& Ks = material.specular;
		Float p1 = Kd.max(), p2 = Ks.max();
		Float total = p1 + p2;
		if (total < eps)break;

		vec3 wo;
		vec3 rad;
		Float prob;
		BxDFType type = material.sample(Xi, ray.d, isct.normal, wo, rad, prob);
		if (BxDFType::none == type) {
			break;
		}
		
		if (type == BxDFType::emission) {
			break;
		}
		v.normal = isct.normal;
		v.wo = wo;
		v.wi = ray.d;
		v.hitpoint = ray.o;
		v.object = object;
		v.type = type;
		v.pdfSA = LightVertex::pdfSolidAngle(path.back(), v);
		v.pdf = LightVertex::pdfArea(path.back(), v);
		v.G = LightVertex::geometryTerm(path.back(), v);
		if (type == BxDFType::diffuse) {
			refl *= std::max(Float(0), -vec3::dotProduct(ray.d, isct.normal));
		}
		
		refl *= rad / prob;
		v.radiance = radiance * refl;
		v.throughput = refl;
		
		ray.d = wo;
		path.emplace_back(v);
		if (type == BxDFType::diffuse) {
			refl *= // 2 * pi;//
				pi / (0.00001 + std::max(Float(0), vec3::dotProduct(wo, isct.normal)));
		}
		Float P = refl.max();
		if (depth > scene->option.rrStartDepth) {
			if (erand48(Xi) < P) {
				refl /= P;
			}
			else {
				break;
			}
		}
		
	}
	path.computePDF();
}

void Miyuki::BDPT::traceEyePath(RenderContext & ctx, Path &path)
{
	Ray& ray = ctx.ray;
	LightVertex v;
	while (ctx.depth < scene->option.maxDepth) {
		intersect(ctx.ray, ctx.intersection);
		if (!ctx.intersection.hit()) {
			break;
		}
		ctx.ray.o += ctx.ray.d * ctx.intersection.distance;
		auto object = ctx.intersection.object;
		auto& material = object->getMaterial();
		vec3 wo;
		vec3 rad;
		Float prob;
		BxDFType type = material.sample(ctx.Xi, ctx.ray.d, ctx.intersection.normal, wo, rad, prob);
		if (prob < eps)
			break;
		if (type == BxDFType::none)break;
		if (type == BxDFType::emission) {
			ctx.color = rad / prob * material.emissionStrength;
			v.normal = ctx.intersection.normal;
			v.wi = ray.d;
			v.wo = wo;
			v.hitpoint = ray.o;
			v.object = object;
			v.type = type;
			v.radiance = ctx.color * ctx.throughput;
			v.throughput = ctx.throughput;
			if (!path.empty()) {
				v.pdfSA = LightVertex::pdfSolidAngle(path.back(), v);
				v.pdf = LightVertex::pdfArea(path.back(), v);
				v.G = LightVertex::geometryTerm(path.back(), v);
			}
			else {
				v.pdfSA = 1 / (0.00001 + fabs(vec3::dotProduct(ray.d, ctx.intersection.normal)));
				v.pdf = v.pdfSA / (ctx.intersection.distance *ctx.intersection.distance);
				v.G = 1;
				v.normal = ctx.intersection.normal;
			}
			path.emplace_back(v);
			break;
		}
		ctx.throughput *= rad / prob;
		v.normal = ctx.intersection.normal;
		v.wi = ray.d;
		v.wo = wo;
		v.hitpoint = ray.o;
		v.object = object;
		v.type = type;
		v.radiance = ctx.color * ctx.throughput;
		v.throughput = ctx.throughput;
		if (!path.empty()) {
			v.pdfSA = LightVertex::pdfSolidAngle(path.back(), v);
			v.pdf = LightVertex::pdfArea(path.back(), v);
			v.G = LightVertex::geometryTerm(path.back(), v);
		}
		else {
			v.pdfSA = 1 / (0.0001 + fabs(vec3::dotProduct(ray.d, ctx.intersection.normal)));
			v.pdf = v.pdfSA / (ctx.intersection.distance *ctx.intersection.distance);
			v.normal = ctx.intersection.normal;
			v.G = 1;
		}
		
		path.emplace_back(v);
		
		if (type == BxDFType::diffuse) {
			ctx.throughput *= pi;
		}
		
		ctx.ray.d = wo;
		const Float P = max(ctx.throughput);
		if (ctx.depth > scene->option.rrStartDepth) {
			if (erand48(ctx.Xi) < P) {
				ctx.throughput /= P;
			}
			else {
				break;
			}
		}
		ctx.depth++;
	}
	//path[0].pdf = path[0].p;
	path.computePDF();
}
static Float computeWeight(Float wc, Float wl) {
	return 1.0 / (wc + wl + 1);
}
vec3 Miyuki::BDPT::connectPath(Path &eye, Path &light)
{
	vec3 radiance(0, 0, 0);
	Float weight = 0;
	for (int i = 0; i < eye.size(); i++) {	
		auto& E = eye[i];
		if (E.type == BxDFType::emission) {
			Float w = MISweight(light, 0, eye, i + 1);
			weight += w;
			radiance += w * E.radiance;
			continue;
		}
		for (int j = 0; j < light.size(); j++) {
			auto& L = light[j];
			if (i + j + 2 > scene->option.maxDepth )
				continue;
			Float c = contribution(E, L);
			if (c > 0) {
				Float w = MISweight(light, j + 1, eye, i + 1);
				if (L.type == BxDFType::emission) {
					radiance += E.throughput
						* c * L.radiance
						* vec3::dotProduct(light.back().normal, light.back().wo) / pi * w;
				}
				else
					radiance += E.throughput * c * L.radiance * w;
				weight += w;
			}
		}
	}
	if (weight <= 1e-18)
		return vec3(0, 0, 0);
	return radiance / weight;
}

Float Miyuki::BDPT::MISweight(Path &L, int s, Path &E, int t)
{
	if (s == 0) {
		if (t == 1)
			return 1;
	}

	Float sumRi = 0;
	Float r = 1;
	for (int i = s - 1; i >= 0; i--) {
		if (i == s - 1 && t > 0) {
			r *= LightVertex::pdfArea(E[t - 1], L[i]) / L[i].pdf;
		}else
			r *= L[i].pdfRev / L[i].pdf;
		sumRi += r;
	}
	r = 1;
	for (int i = t - 1; i >= 0; i--) {
		if (i == t - 1 && s>0) {
			r *= LightVertex::pdfArea(L[s - 1], E[i]) / E[i].pdf;
		}
		else
			r *= E[i].pdfRev / E[i].pdf;
		sumRi += r;
	}
	
	return 1 / (1 + sumRi);
}

vec3 Miyuki::BDPT::trace(int x, int y)
{
	Path eye,light;
	auto ctx = scene->getRenderContext(x, y);
	traceLightPath(ctx.Xi, light);
	traceEyePath(ctx, eye);
	return connectPath(eye, light);
}



bool Miyuki::BDPT::visiblity(const LightVertex & v1, const LightVertex & v2)
{
	Ray ray(v1.hitpoint, (v2.hitpoint - v1.hitpoint).normalized());//from v1 to v2
	Intersection isct;
	isct.exclude = v1.object;
	if (vec3::dotProduct(v1.normal, ray.d) < 0 || -vec3::dotProduct(v2.normal, ray.d) < 0)
		return false;
	intersect(ray, isct);
	return isct.hit() && isct.object == v2.object;
}

Float Miyuki::BDPT::contribution(const LightVertex & v1, const LightVertex & v2)
{
	if ((v1.type == BxDFType::diffuse && v2.type == BxDFType::diffuse)
		|| (v1.type == BxDFType::diffuse && v2.type == BxDFType::emission)){
		if (!visiblity(v1, v2))return 0;
		return LightVertex::geometryTerm(v1, v2);
		/*return G(v1.normal,
			(v2.hitpoint - v1.hitpoint).normalized(),
			v2.normal,
			v1.hitpoint,
			v2.hitpoint);*/
	}
	return 0;
}

void Miyuki::BDPT::generateLightPath()
{
	int N = 1024;
	paths.resize(N);
	Seed Xi []= { 0,rand() };
	parallelFor(0u, (unsigned int)N, [&](unsigned int i) {
		paths[i].clear();
		traceLightPath(Xi, paths[i]);
	});
}

void Miyuki::BDPT::render(Scene *s)
{
	scene = s;
	logger = s->getLogger();
	vec3 dim = scene->getDimension();
	int w = dim.x();
	int h = dim.y();
	//generateLightPath();
	auto limit = 3 * vec3(1, 1, 1);
	parallelFor(0, w, [&](unsigned int i) {
		int x = i;
		for (int y = 0; y < h; y++) {
			scene->getScreen()[x + scene->w * y] *= scene->sampleCount;
			scene->getScreen()[x + scene->w * y] += min(limit, trace(x, y));
			scene->getScreen()[x + scene->w * y] /= 1 + scene->sampleCount;
		}
	});
}

void Miyuki::BDPT::Path::computePDF()
{
	auto&path = *this;
	if (!path.empty()) {
		path.back().pdfRev = 1;
		if (path.back().type == BxDFType::emission) {
			path.back().pdfRev = 1 / path.back().object->area();
		}
	}
	for (int i = path.size() - 2; i >= 0; i--) {
		path[i].pdfRev = LightVertex::pdfArea(path[i + 1], path[i]);
	}
	
}

Float Miyuki::BDPT::LightVertex::pdfOrdinarySolidAngle(const LightVertex & v1, const LightVertex & v2)
{
	auto cos_theta_i = fabs(vec3::dotProduct(v2.hitpoint , v2.normal));
	return LightVertex::pdfSolidAngle(v1, v2)
		* cos_theta_i / (v1.hitpoint - v2.hitpoint).lengthSquared();
}

Float Miyuki::BDPT::LightVertex::pdfSolidAngle(const LightVertex & v1, const LightVertex & v2)
{
	Float p = 0;
	auto cos_wo = fabs(vec3::dotProduct(v1.wo, v1.normal)) / pi;
	if (v1.type == BxDFType::specular) {
		return 1;
	}
	else if (v1.type == BxDFType::diffuse) {
		// cos(theta) / pi
		return pi;
	}
	else if (v1.type == BxDFType::emission) {
		return pi;// / v1.object->area();
	}
	else return 1;
	return p / (0.0001 + cos_wo);
}	

Float Miyuki::BDPT::LightVertex::pdfArea(const LightVertex & v1, const LightVertex & v2)
{
	return LightVertex::pdfSolidAngle(v1, v2) * geometryTerm(v1, v2);
}

Float Miyuki::BDPT::LightVertex::geometryTerm(const LightVertex & v1, const LightVertex & v2)
{
	auto w = v1.hitpoint - v2.hitpoint;
	auto lenSqr = w.lengthSquared();
	w /= sqrt(lenSqr);
	return fabs(vec3::dotProduct(v1.normal, w) * vec3::dotProduct(v2.normal, w)) / (0.001 + lenSqr);

}

Float Miyuki::BDPT::Path::pdfSolidAngle(int i)
{
	auto & path = *this;
	return LightVertex::pdfSolidAngle(path[i - 1], path[i]);
}

Float Miyuki::BDPT::Path::pdfArea(int i)
{
	auto & path = *this;
	return LightVertex::pdfArea(path[i - 1], path[i]);
}

