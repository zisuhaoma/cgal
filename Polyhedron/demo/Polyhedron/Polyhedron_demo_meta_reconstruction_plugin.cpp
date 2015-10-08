#include <QTime>
#include <QApplication>
#include <QAction>
#include <QList>
#include <QMainWindow>
#include <QObject>

#include <fstream>

#include <CGAL/array.h>
#include <CGAL/centroid.h>
#include <CGAL/PCA_util.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/squared_distance_3.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Default_diagonalize_traits.h>
#include <CGAL/compute_average_spacing.h>
#include <CGAL/grid_simplify_point_set.h>
#include <CGAL/jet_smooth_point_set.h>
#include <CGAL/jet_estimate_normals.h>
#include <CGAL/mst_orient_normals.h>
#include <CGAL/Scale_space_surface_reconstruction_3.h>
#include <CGAL/Advancing_front_surface_reconstruction.h>

#include "Scene_polygon_soup_item.h"
#include "Scene_polyhedron_item.h"
#include "Scene_points_with_normal_item.h"
#include "Polyhedron_type.h"

#include "Polyhedron_demo_plugin_helper.h"
#include "Polyhedron_demo_plugin_interface.h"

#include "ui_Polyhedron_demo_meta_reconstruction_plugin.h"

// Poisson reconstruction method:
// Reconstructs a surface mesh from a point set and returns it as a polyhedron.
Polyhedron* poisson_reconstruct(const Point_set& points,
                                Kernel::FT sm_angle, // Min triangle angle (degrees). 
                                Kernel::FT sm_radius, // Max triangle size w.r.t. point set average spacing. 
                                Kernel::FT sm_distance, // Approximation error w.r.t. point set average spacing.
                                const QString& solver, // solver name
                                bool use_two_passes,
				bool do_not_fill_holes);


struct Radius {

  double bound;

  Radius(double bound)
    : bound(bound)
  {}

  bool operator()(const Kernel::Point_3& p, const Kernel::Point_3& q, const Kernel::Point_3& r) const
  {
    if(bound == 0){
      return false;
    }
    double d  = sqrt(squared_distance(p,q));
    if(d>bound) return true;
    d = sqrt(squared_distance(p,r)) ;
    if(d>bound) return true;
    d = sqrt(squared_distance(q,r));
    return d>bound;
  }
};


namespace MetaReconstruction
{
  typedef Kernel::Point_3 Point;
  typedef Kernel::Vector_3 Vector;
  // types for K nearest neighbors search
  typedef CGAL::Search_traits_3<Kernel> Tree_traits;
  typedef CGAL::Orthogonal_k_neighbor_search<Tree_traits> Neighbor_search;
  typedef typename Neighbor_search::Tree Tree;
  typedef typename Neighbor_search::iterator Search_iterator;

  typedef CGAL::Scale_space_surface_reconstruction_3<Kernel> ScaleSpace;
  
  template <typename OutputIterator>
  void generate_scales (OutputIterator out,
			const unsigned int scale_min = 6,
			const double factor = 1.15,
			unsigned int nb_scales = 30)
  {
    unsigned int prev = -1;
    
    for (unsigned int i = 0; i < nb_scales; ++ i)
      {
	unsigned int current = static_cast<unsigned int>(scale_min * std::pow (factor, i));
	if (current != prev)
	  {
	    *(out ++) = current;
	    prev = current;
	  }
	else
	  ++ nb_scales;
      }
  }
  
  unsigned int scale_of_anisotropy (const Point_set& points, double& size)
  {
    Tree tree(points.begin(), points.end());
    
    double ratio_kept = (points.size() < 1000)
      ? 1. : 1000. / (points.size());
    
    std::vector<Point> subset;
    for (std::size_t i = 0; i < points.size (); ++ i)
      if (rand() / (double)RAND_MAX < ratio_kept)
    	subset.push_back (points[i]);
    
    std::vector<unsigned int> scales;
    generate_scales (std::back_inserter (scales));

    std::vector<unsigned int> chosen;

    for (std::size_t i = 0; i < subset.size (); ++ i)
      {
    	Neighbor_search search(tree, subset[i],scales.back());
	double current = 0.;
    	unsigned int nb = 0;
    	std::size_t index = 0;
	double maximum = 0.;
	unsigned int c = 0;
	
    	for (Search_iterator search_iterator = search.begin();
    	     search_iterator != search.end (); ++ search_iterator, ++ nb)
    	  {
	    current += search_iterator->second;

    	    if (nb + 1 == scales[index])
    	      {
		double score = std::sqrt (current / scales[index])
		  / std::pow (scales[index], 0.75); // NB ^ (3/4)

		if (score > maximum)
		  {
		    maximum = score;
		    c = scales[index];
		  }

    		++ index;
    		if (index == scales.size ())
    		  break;
    	      }
    	  }
	chosen.push_back (c);
      }

    double mean = 0.;
    for (std::size_t i = 0; i < chosen.size(); ++ i)
      mean += chosen[i];
    mean /= chosen.size();
    unsigned int aniso_scale = static_cast<unsigned int>(mean);

    size = 0.;
    for (std::size_t i = 0; i < subset.size (); ++ i)
      {
    	Neighbor_search search(tree, subset[i], aniso_scale);
	size += std::sqrt ((-- search.end())->second);
      }
    size /= subset.size();
    
    return aniso_scale;
  }

  
  unsigned int scale_of_noise (const Point_set& points, double& size)
  {
    Tree tree(points.begin(), points.end());
    
    double ratio_kept = (points.size() < 1000)
      ? 1. : 1000. / (points.size());
    
    std::vector<Point> subset;
    for (std::size_t i = 0; i < points.size (); ++ i)
      if (rand() / (double)RAND_MAX < ratio_kept)
    	subset.push_back (points[i]);
    
    std::vector<unsigned int> scales;
    generate_scales (std::back_inserter (scales));

    std::vector<unsigned int> chosen;
    
    for (std::size_t i = 0; i < subset.size (); ++ i)
      {
    	Neighbor_search search(tree, subset[i],scales.back());
	double current = 0.;
    	unsigned int nb = 0;
    	std::size_t index = 0;
	double minimum = (std::numeric_limits<double>::max)();
	unsigned int c = 0;
	
    	for (Search_iterator search_iterator = search.begin();
    	     search_iterator != search.end (); ++ search_iterator, ++ nb)
    	  {
	    current += search_iterator->second;

    	    if (nb + 1 == scales[index])
    	      {
		double score = std::sqrt (current / scales[index])
		  / std::pow (scales[index], 0.375); // NB ^ (5/12)

		if (score < minimum)
		  {
		    minimum = score;
		    c = scales[index];
		  }

    		++ index;
    		if (index == scales.size ())
    		  break;
    	      }
    	  }
	chosen.push_back (c);
      }

    std::sort (chosen.begin (), chosen.end());

    unsigned int noise_scale = chosen[chosen.size() / 2];
    size = 0.;
    for (std::size_t i = 0; i < subset.size (); ++ i)
      {
    	Neighbor_search search(tree, subset[i], noise_scale);
	size += std::sqrt ((-- search.end())->second);
      }
    size /= subset.size();

    
    return noise_scale;
  }

  void simplify_point_set (Point_set& points, double size)
  {
    points.erase (CGAL::grid_simplify_point_set (points.begin (), points.end (), size),
		  points.end ());
  }

  void smooth_point_set (Point_set& points, unsigned int scale)
  {
    CGAL::jet_smooth_point_set<CGAL::Parallel_tag>(points.begin(), points.end(),
                                                   scale);
  }

  void scale_space (const Point_set& points, Scene_polygon_soup_item* new_item,
		    unsigned int scale, bool interpolate = true)
  {
    ScaleSpace reconstruct (scale, 300);
    reconstruct.reconstruct_surface(points.begin (), points.end (), 4, false, true);

    new_item->init_polygon_soup(points.size(), reconstruct.number_of_triangles ());

    std::map<unsigned int, unsigned int> map_i2i;
    unsigned int current_index = 0;
    
    for (ScaleSpace::Triple_iterator it = reconstruct.surface_begin ();
	 it != reconstruct.surface_end (); ++ it)
      {
	for (unsigned int ind = 0; ind < 3; ++ ind)
	  {
	    if (map_i2i.find ((*it)[ind]) == map_i2i.end ())
	      {
		map_i2i.insert (std::make_pair ((*it)[ind], current_index ++));
		Point p = (interpolate ? points[(*it)[ind]].position() : *(reconstruct.points_begin() + (*it)[ind]));
		new_item->new_vertex (p.x (), p.y (), p.z ());
	      }
	  }
	new_item->new_triangle( map_i2i[(*it)[0]],
				map_i2i[(*it)[1]],
				map_i2i[(*it)[2]] );
      }
  }

  void advancing_front (const Point_set& points, Scene_polyhedron_item* new_item, double size)
  {
    Polyhedron& P = * const_cast<Polyhedron*>(new_item->polyhedron());
    Radius filter(10 * size);

    CGAL::advancing_front_surface_reconstruction (points.begin (), points.end (), P, filter);
						  
  }

  void compute_normals (Point_set& points, unsigned int neighbors)
  {
    CGAL::jet_estimate_normals<CGAL::Parallel_tag>(points.begin(), points.end(),
                                                   CGAL::make_normal_of_point_with_normal_pmap(Point_set::value_type()),
                                                   2 * neighbors);
    
    points.erase (CGAL::mst_orient_normals (points.begin(), points.end(),
					    CGAL::make_normal_of_point_with_normal_pmap(Point_set::value_type()),
					    2 * neighbors),
		  points.end ());
  }
  
}

class Polyhedron_demo_meta_reconstruction_plugin_dialog : public QDialog, private Ui::MetaReconstructionOptionsDialog
{
  Q_OBJECT
public:
  Polyhedron_demo_meta_reconstruction_plugin_dialog(QWidget* /*parent*/ = 0)
  {
    setupUi(this);
  }

  bool boundaries() const { return m_boundaries->isChecked(); }
  bool interpolate() const { return m_interpolate->isChecked(); }
};

#include <CGAL/Scale_space_surface_reconstruction_3.h>

class Polyhedron_demo_meta_reconstruction_plugin :
  public QObject,
  public Polyhedron_demo_plugin_helper
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "com.geometryfactory.PolyhedronDemo.PluginInterface/1.0")

  Q_INTERFACES(Polyhedron_demo_plugin_interface)
  QAction* actionScaleSpaceReconstruction;

public:
  void init(QMainWindow* mainWindow, Scene_interface* scene_interface) {

    actionScaleSpaceReconstruction = new QAction(tr("Meta surface reconstruction"), mainWindow);
    actionScaleSpaceReconstruction->setObjectName("actionMetaReconstruction");

    Polyhedron_demo_plugin_helper::init(mainWindow, scene_interface);
  }

  //! Applicate for Point_sets with normals.
  bool applicable(QAction*) const {
    return qobject_cast<Scene_points_with_normal_item*>(scene->item(scene->mainSelectionIndex()));
  }

  QList<QAction*> actions() const {
    return QList<QAction*>() << actionScaleSpaceReconstruction;
  }

public Q_SLOTS:
  void on_actionMetaReconstruction_triggered();
}; // end class Polyhedron_meta_reconstruction_plugin


void Polyhedron_demo_meta_reconstruction_plugin::on_actionMetaReconstruction_triggered()
{
  const Scene_interface::Item_id index = scene->mainSelectionIndex();

  Scene_points_with_normal_item* pts_item =
    qobject_cast<Scene_points_with_normal_item*>(scene->item(index));

  if(pts_item)
    {
      // Gets point set
      Point_set* points = pts_item->point_set();

      //generate the dialog box to set the options
      Polyhedron_demo_meta_reconstruction_plugin_dialog dialog;
      if(!dialog.exec())
	return;

      // wait cursor
      QApplication::setOverrideCursor(Qt::WaitCursor);

      QTime time;
      std::cout << "Meta surface reconstruction with the following requirements:" << std::endl
		<< (dialog.boundaries() ? " * Output shape has boundaries" : " * Output shape is closed") << std::endl
		<< (dialog.interpolate() ? " * Output shape passes through input points"
		    : " * Output shape approximates input points") << std::endl;

      Scene_points_with_normal_item* new_item = NULL;
      if (!(dialog.interpolate()))
	{
	  new_item = new Scene_points_with_normal_item();
	  new_item->setName(QString("%1 (preprocessed)").arg(pts_item->name()));
	  new_item->set_has_normals (pts_item->has_normals());
	  new_item->setColor(pts_item->color());
	  new_item->setRenderingMode(pts_item->renderingMode());
	  new_item->setVisible(pts_item->visible());
	  new_item->resetSelection();
	  new_item->invalidate_buffers();

	  points = new_item->point_set();
	  std::copy (pts_item->point_set()->begin(), pts_item->point_set()->end(),
		     std::back_inserter (*points));
	}

      std::cerr << "Analysing isotropy of point set... ";
      time.start();
      double aniso_size;
      unsigned int aniso_scale = MetaReconstruction::scale_of_anisotropy (*points, aniso_size);
      std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;

      std::cerr << " -> Scale / size = " << aniso_scale << " / " << aniso_size << std::endl;
      
      bool isotropic = (aniso_scale == 6);
      std::cerr << (isotropic ? " -> Point set is isotropic" : " -> Point set is anisotropic") << std::endl;
      if (!(dialog.interpolate()) && !isotropic)
	{
	  std::cerr << "Correcting anisotropy of point set... ";
	  time.restart();
	  std::size_t prev_size = points->size ();
	  MetaReconstruction::simplify_point_set (*points, aniso_size);
	  std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
	  std::cerr << " -> " << prev_size - points->size() << " point(s) removed ("
		    << 100. * (prev_size - points->size()) / (double)(prev_size)
		    << "%)" << std::endl;
	}

      std::cerr << "Analysing noise of point set... ";
      time.restart();
      double noise_size;
      unsigned int noise_scale = MetaReconstruction::scale_of_noise (*points, noise_size);
      std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
      
      std::cerr << " -> Scale / size = " << noise_scale << " / " << noise_size << std::endl;
      
      bool noisy = (noise_scale > 6);
      std::cerr << (noisy ? " -> Point set is noisy" : " -> Point set is noise-free") << std::endl;
      
      if (!(dialog.interpolate()) && noisy)
	{
	  std::cerr << "Denoising point set... ";
	  time.restart();
	  MetaReconstruction::smooth_point_set (*points, noise_scale);
          new_item->set_has_normals (false);
	  std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
	}

      if (dialog.interpolate())
	{
	  if (noisy)
	    { 
	      std::cerr << "Scale space reconstruction... ";
	      time.restart();

	      Scene_polygon_soup_item* reco_item = new Scene_polygon_soup_item();
	      MetaReconstruction::scale_space (*points, reco_item, (std::max)(noise_scale, aniso_scale));
	      
	      reco_item->setName(tr("%1 (scale space interpolant)").arg(scene->item(index)->name()));
	      reco_item->setColor(Qt::magenta);
	      reco_item->setRenderingMode(FlatPlusEdges);
	      scene->addItem(reco_item);

	      std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
	    }
	  else
	    {
	      std::cerr << "Advancing front reconstruction... ";
	      time.restart();

	      Scene_polyhedron_item* reco_item = new Scene_polyhedron_item(Polyhedron());
	      MetaReconstruction::advancing_front (*points, reco_item, (std::max)(noise_size, aniso_size));
	      
	      reco_item->setName(tr("%1 (advancing front)").arg(scene->item(index)->name()));
	      reco_item->setColor(Qt::magenta);
	      reco_item->setRenderingMode(FlatPlusEdges);
	      scene->addItem(reco_item);

	      std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
	    }

	}
      else
	{
	  if (dialog.boundaries())
	    {
	      std::cerr << "Scale space reconstruction... ";
	      time.restart();

	      Scene_polygon_soup_item* reco_item = new Scene_polygon_soup_item();
	      MetaReconstruction::scale_space (*points, reco_item, noise_scale, false);

	      reco_item->setName(tr("%1 (scale space smoothed)").arg(scene->item(index)->name()));
	      reco_item->setColor(Qt::magenta);
	      reco_item->setRenderingMode(FlatPlusEdges);
	      scene->addItem(reco_item);

	      std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
	    }
	  else
	    {
	      if (!(new_item->has_normals()))
		{
		  std::cerr << "Estimation of normal vectors... ";
		  time.restart();

		  MetaReconstruction::compute_normals (*points, noise_scale);
		  
		  new_item->set_has_normals (true);
		  new_item->setRenderingMode(PointsPlusNormals);
		  
		  std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
		}
	      
	      std::cerr << "Poisson reconstruction... ";
	      time.restart();

	      Polyhedron* pRemesh = poisson_reconstruct(*points, 20,
							100 * (std::max)(noise_size, aniso_size),
							(std::max)(noise_size, aniso_size),
							QString ("Eigen - built-in CG"), false, false);
	      if(pRemesh)

		{
		  // Add polyhedron to scene
		  Scene_polyhedron_item* reco_item = new Scene_polyhedron_item(pRemesh);
		  reco_item->setName(tr("%1 (poisson)").arg(pts_item->name()));
		  reco_item->setColor(Qt::lightGray);
		  scene->addItem(reco_item);
		}

	      std::cerr << "ok (" << time.elapsed() << " ms)" << std::endl;
	    }
	}

      if (!(dialog.interpolate()))
	{
	  if (noisy || !isotropic)
	    scene->addItem(new_item);
	  else
	    delete new_item;
	}

      // default cursor
      QApplication::restoreOverrideCursor();
    }
}

#include "Polyhedron_demo_meta_reconstruction_plugin.moc"
